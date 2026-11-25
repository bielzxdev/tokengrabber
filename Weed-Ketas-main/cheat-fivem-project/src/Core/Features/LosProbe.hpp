#pragma once
#include <Includes/Includes.hpp>
#include <Core/SDK/Memory.hpp>
#include <algorithm>
#include <cstring>
#include <vector>
#include <chrono>
#include <mutex>

namespace Core {
    namespace Features {

        // ============================================================
        // LOS probe: chama CPed::OurPedCanSeeThisEntity no processo do jogo.
        //
        // O native HAS_ENTITY_CLEAR_LOS_TO_ENTITY, quando a entidade 1 e um
        // ped (nosso caso), NAO faz shape test proprio -- ele desvia para:
        //
        //   ?OurPedCanSeeThisEntity@CPed@@QEBA_NPEBVCEntity@@_NI1@Z
        //
        // A Steam e a b3258 usam ABIs diferentes. Confirmado decompilando
        // ambas as funcoes nos dumps abertos no IDA:
        //
        //   bool f(CPed*    this           /*RCX*/,
        //          CEntity* pEnt           /*RDX*/,
        //          float*   vOffset        /*R8*/,          <-- EXTRA
        //          char     bForTargeting  /*R9B*/,
        //          int      nIncludeFlags  /*[rsp+0x20]*/,
        //          char     bAdjustForCover/*[rsp+0x28]*/)
        //
        // A b3258 tem SEIS argumentos. Na Steam sao CINCO: R8 e o bool,
        // R9 e nIncludeFlags, e bAdjustForCover fica em [rsp+0x20].
        // Confundir as duas ABIs pode desreferenciar 0x1 na b3258.
        //
        // Retorno: SubmitTest(...) == 0, ou seja 1 = LOS livre.
        //
        // bForTargeting = true faz a funcao pular a checagem de cone de visao
        // / alcance de percepcao do ped, deixando so a geometria.
        //
        // nIncludeFlags vem de shapetest_commands::GetPhysicsFlags(LOSFlags).
        // Para o LOSFlags 17 que os scripts usam: bit0 -> 4, bit4 -> 0x2000,
        // logo 0x2004.
        //
        // Em vez de criar uma thread por chamada (o que a versao antiga fazia:
        // ~1ms cada e e o padrao que os anticheats mais olham), criamos UMA
        // thread que fica dormindo e processa pedidos por uma mailbox.
        // ============================================================
        class cLosProbe {
        private:
            // layout da mailbox (inicio do cave)
            enum : uintptr_t {
                MB_REQUEST = 0x00,  // u32  1 = ha pedido
                MB_DONE = 0x04,  // u32  1 = resposta pronta
                MB_FROM = 0x08,  // u64
                MB_TO = 0x10,  // u64
                MB_FLAGS = 0x18,  // u32
                MB_RESULT = 0x1C,  // u32
                MB_LOSFN = 0x20,  // u64
                MB_SLEEPFN = 0x28,  // u64
                MB_OFFSET = 0x30,  // Vector3 zerado, passado como vOffset
                MB_SEQ = 0x40,  // u32 sequencia do pedido
                MB_ECHO = 0x44,  // u32 sequencia que o worker de fato processou
                MB_CODE = 0x1000,  // shellcode na 2a pagina (RX), mailbox fica RW na 1a
            };

            // ~30k PAUSE ≈ centenas de microssegundos de espera ativa. Cobre
            // com folga o intervalo entre pedidos enquanto o ESP roda, e o
            // Sleep so entra quando nao ha nada a fazer -- assim nao fica
            // queimando um nucleo do jogo a toa.
            static constexpr uint32_t kSpinCount = 30000;

            enum class LosAbi : uint8_t { Unknown, FiveArgs, SixArgs };

            uintptr_t m_cave = 0;
            uintptr_t m_losFn = 0;
            uintptr_t m_sleepFn = 0;
            LosAbi    m_abi = LosAbi::Unknown;
            bool      m_ready = false;
            // Antes isso era um latch permanente: uma falha e o raycast
            // morria pelo resto da sessao. A thread do EntityList sobe cedo,
            // entao a primeira tentativa pode acontecer antes do codigo do
            // jogo estar mapeado -- e ai nunca mais tentava.
            int  m_failCount = 0;
            std::chrono::steady_clock::time_point m_nextTry{};
            bool      m_pending = false;   // ha um pedido em voo
            uint32_t  m_seq = 0;           // sequencia do pedido atual
            uint32_t  m_mismatch = 0;      // respostas descartadas por sequencia
            // contadores de diagnostico
            uint32_t  m_posts = 0, m_oks = 0, m_timeouts = 0;
            uint32_t  m_clear = 0, m_blocked = 0;   // resposta crua da funcao
            uint32_t  m_polls = 0;                 // voltas de Collect gastas
            std::chrono::steady_clock::time_point m_postedAt{};
            std::mutex m_mutex;

            // A alocacao de registradores muda entre builds (a de referencia
            // usava rbx/r12/r15, a do usuario usa r13/r12/r15 em outra ordem),
            // entao a assinatura e ESTRUTURAL, com os registradores em curinga:
            //
            //   48 8D A8 ?? ?? FF FF   lea    rbp, [rax-0x9xx]   frame gigante
            //   48 81 EC ?? ?? 00 00   sub    rsp, 0x9xx
            //   0F 28 ?? 90 00 00 00   movaps xmmA, [rdx+0x90]   pos do alvo
            //   0F 28 ?? 70            movaps xmmB, [rcx+0x70]
            //
            // Casa em funcStart + 0x18 e e unica no dump da build do usuario.
            static constexpr uintptr_t kSigToFuncStart = 0x18;

            // E o prologo da propria funcao, conferido em separado.
            static bool ValidatePrologue(uintptr_t fn) {
                static const uint8_t Expect[] = {
                    0x48, 0x8B, 0xC4,                    // mov rax, rsp
                    0x48, 0x89, 0x58, 0x08,              // mov [rax+8], rbx
                    0x48, 0x89, 0x70, 0x10,              // mov [rax+10h], rsi
                    0x48, 0x89, 0x78, 0x18,              // mov [rax+18h], rdi
                    0x55, 0x41, 0x54, 0x41, 0x55,        // push rbp,r12,r13
                    0x41, 0x56, 0x41, 0x57               // push r14,r15
                };
                std::vector<uint8_t> Got = Core::Mem.ReadBytes(fn, sizeof(Expect));
                return Got.size() == sizeof(Expect)
                    && std::memcmp(Got.data(), Expect, sizeof(Expect)) == 0;
            }

            // A assinatura estrutural e o prologo sao iguais nos dois dumps.
            // Conferir como o terceiro e o quarto argumentos sao usados antes
            // de montar a chamada. Os pares abaixo foram verificados no IDA:
            // Steam: mov r15b,r8b + test r8b,r8b (5 argumentos).
            // b3258: mov r15,r8  + test r9b,r9b (6 argumentos).
            // Uma variante nova fica indisponivel ate ter sua ABI confirmada.
            static LosAbi DetectAbi(uintptr_t fn) {
                const auto Code = Core::Mem.ReadBytes(fn, 0xA0);
                if (Code.size() != 0xA0) return LosAbi::Unknown;
                auto Has = [&](std::initializer_list<uint8_t> Pattern) {
                    return std::search(Code.begin(), Code.end(),
                        Pattern.begin(), Pattern.end()) != Code.end();
                };
                const bool Five = Has({ 0x45, 0x8A, 0xF8 }) &&
                    Has({ 0x45, 0x84, 0xC0 });
                const bool Six = Has({ 0x4D, 0x8B, 0xF8 }) &&
                    Has({ 0x45, 0x84, 0xC9 });
                if (Five == Six) return LosAbi::Unknown;
                return Five ? LosAbi::FiveArgs : LosAbi::SixArgs;
            }

            // Monta o loop do worker com os saltos calculados, em vez de
            // contar bytes na mao.
            std::vector<uint8_t> BuildWorker(uintptr_t mailbox) const {
                std::vector<uint8_t> C;
                auto Emit = [&](std::initializer_list<uint8_t> B) { C.insert(C.end(), B); };

                // mov rbx, <mailbox>
                Emit({ 0x48, 0xBB });
                for (int i = 0; i < 8; i++) C.push_back((uint8_t)(mailbox >> (i * 8)));

                // and rsp, -16   (garante alinhamento de 16 para as chamadas)
                Emit({ 0x48, 0x83, 0xE4, 0xF0 });

                // Orcamento de espera ativa antes de recorrer ao Sleep.
                // Sleep(1) sozinho custava ate ~15ms por pedido, porque a
                // resolucao de timer padrao do Windows e 15.6ms -- era essa a
                // maior fonte de atraso do raycast inteiro.
                auto EmitSpinReset = [&]() {
                    Emit({ 0x41, 0xBB });                 // mov r11d, imm32
                    for (int i = 0; i < 4; i++) C.push_back((uint8_t)(kSpinCount >> (i * 8)));
                };

                EmitSpinReset();

                const size_t Loop = C.size();

                Emit({ 0x8B, 0x03 });               // mov  eax, [rbx]
                Emit({ 0x85, 0xC0 });               // test eax, eax
                Emit({ 0x75, 0x00 });               // jne  -> work  (patch)
                const size_t JneOperand = C.size() - 1;

                // --- ocioso: gira com PAUSE enquanto houver orcamento ---
                Emit({ 0xF3, 0x90 });               // pause
                Emit({ 0x41, 0x83, 0xEB, 0x01 });   // sub  r11d, 1
                Emit({ 0x75, 0x00 });               // jne  -> loop  (patch)
                const size_t JneSpinOperand = C.size() - 1;
                C[JneSpinOperand] = (uint8_t)(int8_t)((int)Loop - (int)(JneSpinOperand + 1));

                // --- orcamento esgotado: dorme e recarrega ---
                Emit({ 0xB9, 0x01, 0x00, 0x00, 0x00 });   // mov ecx, 1
                Emit({ 0x48, 0x8B, 0x43, MB_SLEEPFN });   // mov rax, [rbx+28]
                Emit({ 0x48, 0x83, 0xEC, 0x20 });         // sub rsp, 20h
                Emit({ 0xFF, 0xD0 });                     // call rax
                Emit({ 0x48, 0x83, 0xC4, 0x20 });         // add rsp, 20h
                EmitSpinReset();
                Emit({ 0xEB, 0x00 });                     // jmp loop (patch)
                const size_t JmpIdleOperand = C.size() - 1;
                C[JmpIdleOperand] = (uint8_t)(int8_t)((int)Loop - (int)(JmpIdleOperand + 1));

                // --- trabalho ---
                const size_t Work = C.size();
                C[JneOperand] = (uint8_t)(int8_t)((int)Work - (int)(JneOperand + 1));

                // Chamada com a ABI identificada no codigo da build atual.
                //
                // Na ABI de seis argumentos, R10 guarda flags antes de montar
                // RCX/RDX/R8/R9. Na de cinco, R9 recebe flags diretamente.
                // Captura a sequencia no INICIO. r15d e callee-saved, entao a
                // funcao do jogo preserva -- e capturar no fim leria uma
                // sequencia que ja poderia ter sido trocada por um Post novo.
                Emit({ 0x44, 0x8B, 0x7B, MB_SEQ });           // mov  r15d, [rbx+40]
                Emit({ 0x48, 0x8B, 0x4B, MB_FROM });          // mov  rcx, [rbx+08]  this
                Emit({ 0x48, 0x8B, 0x53, MB_TO });            // mov  rdx, [rbx+10]  pEntity
                if (m_abi == LosAbi::SixArgs) {
                    Emit({ 0x44, 0x8B, 0x53, MB_FLAGS });     // mov r10d, [rbx+18]
                    Emit({ 0x4C, 0x8D, 0x43, MB_OFFSET });    // lea r8, [rbx+30]
                    Emit({ 0x41, 0xB9, 0x01, 0x00, 0x00, 0x00 }); // mov r9d, 1
                }
                else {
                    Emit({ 0x41, 0xB8, 0x01, 0x00, 0x00, 0x00 }); // mov r8d, 1
                    Emit({ 0x44, 0x8B, 0x4B, MB_FLAGS });     // mov r9d, [rbx+18]
                }
                Emit({ 0x48, 0x8B, 0x43, MB_LOSFN });         // mov  rax, [rbx+20]

                // 0x20 de shadow space + ate dois argumentos na pilha.
                // 0x40 mantem o RSP alinhado em 16 na hora do call
                // (0x38 nao manteria: 56 % 16 = 8).
                Emit({ 0x48, 0x83, 0xEC, 0x40 });                         // sub rsp, 40h
                if (m_abi == LosAbi::SixArgs)
                    Emit({ 0x44, 0x89, 0x54, 0x24, 0x20 });               // arg5: flags
                else
                    Emit({ 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00 }); // arg5: cover=false
                if (m_abi == LosAbi::SixArgs)
                    Emit({ 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00 }); // arg6: cover=false
                Emit({ 0xFF, 0xD0 });                                     // call rax
                Emit({ 0x48, 0x83, 0xC4, 0x40 });                         // add rsp, 40h
                Emit({ 0x0F, 0xB6, 0xC0 });               // movzx eax, al
                Emit({ 0x89, 0x43, MB_RESULT });          // mov [rbx+1C], eax
                Emit({ 0x44, 0x89, 0x7B, MB_ECHO });      // mov [rbx+44], r15d
                Emit({ 0xC7, 0x03, 0x00, 0x00, 0x00, 0x00 });             // mov [rbx], 0
                Emit({ 0xC7, 0x43, MB_DONE, 0x01, 0x00, 0x00, 0x00 });    // mov [rbx+04], 1

                // Acabou de trabalhar: recarrega o giro para o proximo pedido
                // ser atendido na hora, em vez de cair no Sleep.
                EmitSpinReset();

                Emit({ 0xE9, 0x00, 0x00, 0x00, 0x00 });   // jmp loop (rel32)
                const size_t Rel32 = C.size() - 4;
                int32_t Delta = (int32_t)((int)Loop - (int)(Rel32 + 4));
                std::memcpy(C.data() + Rel32, &Delta, 4);

                return C;
            }

        public:
            bool Available() const { return m_ready; }
            bool SupportsOffset() const { return m_ready && m_abi == LosAbi::SixArgs; }

            struct Stats { bool Ready; int Fails; uint32_t Posts, Oks, Timeouts, Clear, Blocked, Mismatch, Polls; };
            Stats GetStats() {
                std::lock_guard<std::mutex> Lock(m_mutex);
                return { m_ready, m_failCount, m_posts, m_oks, m_timeouts, m_clear, m_blocked, m_mismatch, m_polls };
            }

            bool Initialize() {
                std::lock_guard<std::mutex> Lock(m_mutex);
                if (m_ready) return true;

                // O codigo do jogo pode ser mapeado depois da primeira passada.
                const auto Now = std::chrono::steady_clock::now();
                if (Now < m_nextTry)   return false;

                m_nextTry = Now + std::chrono::milliseconds(
                    m_failCount < 20 ? 500 : 5000);
                m_failCount++;

                // NAO usar a assinatura de forum "57 48 83 EC 60 48 8B 01 ...":
                // ela casa bytes mas e um getter de posicao de osso, onde RDX
                // e ponteiro de SAIDA -- chamar com RDX = ped alvo escreveria
                // 16 bytes sobre o vtable do ped a cada consulta.
                const std::string Sig =
                    xorstr("48 8D A8 ? ? FF FF 48 81 EC ? ? 00 00 0F 28 ? 90 00 00 00 0F 28 ? 70");

                uintptr_t Hit = Core::Mem.FindSignatureStr(Sig);
                if (!Hit) return false;

                // A assinatura tem de ser UNICA. Se casar num segundo lugar,
                // nao da para saber qual e a funcao certa -- desiste.
                const uintptr_t Rest = Core::Mem.ModBase + Core::Mem.ModBaseSize;
                if (Hit + 1 < Rest &&
                    Core::Mem.FindSignatureStr(Sig, Hit + 1, Rest - (Hit + 1)))
                    return false;

                m_losFn = Hit - kSigToFuncStart;
                if (!ValidatePrologue(m_losFn))
                    return false;
                m_abi = DetectAbi(m_losFn);
                if (m_abi == LosAbi::Unknown)
                    return false;

                uintptr_t Sleep_ = (uintptr_t)GetProcAddress(
                    GetModuleHandleA(xorstr("kernel32.dll")), xorstr("Sleep"));
                if (!Sleep_) return false;

                m_cave = Core::Mem.CreateCodeCave(0x2000);
                if (!m_cave) return false;

                // mailbox zerada + ponteiros das funcoes
                std::vector<uint8_t> Zero(MB_CODE, 0);
                Core::Mem.WriteBytes(m_cave, Zero);
                Core::Mem.Write<uint64_t>(m_cave + MB_LOSFN, (uint64_t)m_losFn);
                Core::Mem.Write<uint64_t>(m_cave + MB_SLEEPFN, (uint64_t)Sleep_);
                m_sleepFn = Sleep_;

                std::vector<uint8_t> Code = BuildWorker(m_cave);
                if (!Core::Mem.WriteBytes(m_cave + MB_CODE, Code)) {
                    Core::Mem.FreeCave(m_cave);
                    m_cave = 0;
                    return false;
                }
                Core::Mem.SealCave(m_cave + MB_CODE, 0x1000);

                static auto pCreate = (pNtCreateThreadEx)SyscallHelper::CreateStub(xorstr("NtCreateThreadEx"));
                if (!pCreate) { Core::Mem.FreeCave(m_cave); m_cave = 0; return false; }

                HANDLE hThread = nullptr;
                pCreate(&hThread, THREAD_ALL_ACCESS, nullptr, Core::Mem.ProcHandle,
                    (LPTHREAD_START_ROUTINE)(m_cave + MB_CODE),
                    nullptr, FALSE, 0, 0, 0, nullptr);

                if (!hThread) { Core::Mem.FreeCave(m_cave); m_cave = 0; return false; }
                CloseHandle(hThread); // o worker vive sozinho; nao esperamos por ele

                m_failCount = 0;
                m_ready = true;
                return true;
            }

            // -1 = sem resposta a tempo (o chamador decide o fallback)
            //  0 = linha de visao bloqueada
            //  1 = linha de visao livre
            //
            // traceFlags aqui ja e o nIncludeFlags de fisica, nao o LOSFlags
            // de script: GetPhysicsFlags(17) = 4 | 0x2000 = 0x2004.
            // ==========================================================
            // API ASSINCRONA.
            //
            // A versao anterior bloqueava esperando a resposta. Mesmo com o
            // worker respondendo rapido, cada volta do polling e uma syscall
            // (ZwReadVirtualMemory, ~5-20us), entao 4 consultas por passada
            // seguravam a thread do EntityList por milissegundos -- e era dai
            // que vinha o atraso do ESP inteiro.
            //
            // Agora: posta o pedido e sai. O resultado e recolhido numa
            // passada seguinte. A resposta fica ~1 passada atrasada, o que e
            // irrelevante para visibilidade, e a thread nunca para.
            // ==========================================================

            bool Busy() {
                if (!m_ready) return false;
                std::lock_guard<std::mutex> Lock(m_mutex);
                return m_pending;
            }

            // Posta um pedido. Devolve false se ja ha um em voo.
            bool Post(uintptr_t from, uintptr_t to, float offsetZ = 0.f, uint32_t traceFlags = 0x2004) {
                return PostOffset(from, to, 0.f, 0.f, offsetZ, traceFlags);
            }

            // A build b3258 recebe vOffset em R8. O caminho de ped acima
            // conserva a API antiga; veiculos podem mirar um ponto 3D exato.
            bool PostOffset(uintptr_t from, uintptr_t to, float offsetX,
                float offsetY, float offsetZ, uint32_t traceFlags = 0x2004) {
                if (!m_ready || !from || !to) return false;

                std::lock_guard<std::mutex> Lock(m_mutex);
                if (m_pending) return false;

                m_seq++;

                // Um bloco so, em vez de dez escritas avulsas.
                //
                // Cada Write e uma syscall (~10us). Eram 10 por consulta, e com
                // ate 42 consultas por passada isso sozinho passava de 3ms --
                // passada lenta significa leitura de visibilidade velha, que e
                // o que gera tanto atraso quanto piscar.
                //
                // REQUEST fica de fora e vai por ultimo: e o sinal que libera o
                // worker, e ele nao pode ver o pedido antes dos dados estarem
                // no lugar.
                uint8_t Blk[MB_ECHO + 4 - MB_DONE]{};
                auto Put32 = [&](uintptr_t Off, uint32_t V) {
                    std::memcpy(Blk + (Off - MB_DONE), &V, 4); };
                auto Put64 = [&](uintptr_t Off, uint64_t V) {
                    std::memcpy(Blk + (Off - MB_DONE), &V, 8); };

                Put32(MB_DONE, 0);
                Put64(MB_FROM, (uint64_t)from);
                Put64(MB_TO, (uint64_t)to);
                Put32(MB_FLAGS, traceFlags);
                Put32(MB_RESULT, 0);

                // Esses dois ficam no meio do bloco: precisam ser reescritos
                // com o valor correto, senao a escrita em bloco os zeraria e o
                // worker chamaria o endereco 0.
                Put64(MB_LOSFN, (uint64_t)m_losFn);
                Put64(MB_SLEEPFN, (uint64_t)m_sleepFn);

                std::memcpy(Blk + (MB_OFFSET + 0 - MB_DONE), &offsetX, 4);
                std::memcpy(Blk + (MB_OFFSET + 4 - MB_DONE), &offsetY, 4);
                std::memcpy(Blk + (MB_OFFSET + 8 - MB_DONE), &offsetZ, 4);

                Put32(MB_SEQ, m_seq);
                Put32(MB_ECHO, 0);

                Core::Mem.WriteBytes(m_cave + MB_DONE,
                    std::vector<uint8_t>(Blk, Blk + sizeof(Blk)));

                Core::Mem.Write<uint32_t>(m_cave + MB_REQUEST, 1);

                m_pending = true;
                m_posts++;
                m_postedAt = std::chrono::steady_clock::now();
                return true;
            }

            // -1 = nada pronto ainda. 0 = bloqueado. 1 = linha de visao livre.
            int Collect() {
                if (!m_ready) return -1;

                std::lock_guard<std::mutex> Lock(m_mutex);
                if (!m_pending) return -1;
                m_polls++;

                if (Core::Mem.Read<uint32_t>(m_cave + MB_DONE) == 1) {
                    m_pending = false;

                    // Resposta de um pedido anterior que so voltou agora:
                    // atribui-la ao pedido atual colaria o resultado de um ped
                    // em outro. Descarta.
                    if (Core::Mem.Read<uint32_t>(m_cave + MB_ECHO) != m_seq) {
                        m_mismatch++;
                        return -1;
                    }

                    m_oks++;
                    const int R = Core::Mem.Read<uint32_t>(m_cave + MB_RESULT) ? 1 : 0;
                    if (R) m_clear++; else m_blocked++;
                    return R;
                }

                // Worker morreu ou travou: libera o slot para nao ficar preso
                // para sempre num pedido que nunca volta.
                const auto Age = std::chrono::steady_clock::now() - m_postedAt;
                if (Age > std::chrono::milliseconds(50)) {
                    Core::Mem.Write<uint32_t>(m_cave + MB_REQUEST, 0);
                    m_pending = false;
                    m_timeouts++;
                }

                return -1;
            }
        };

        inline cLosProbe g_LosProbe;
    }
}
