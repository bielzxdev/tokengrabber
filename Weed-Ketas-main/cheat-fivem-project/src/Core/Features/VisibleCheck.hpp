#pragma once
#include <Includes/Includes.hpp>
#include <Core/SDK/Memory.hpp>
#include <Core/Offsets.hpp>
#include <Core/Features/LosProbe.hpp>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <vector>
#include <utility>
#include <Windows.h>

namespace Core {
    namespace Features {
        namespace vischeckoffsets {
            constexpr uint32_t entityflags = 0xc0;
            constexpr uint32_t visualptr = 0x48;
            constexpr uint32_t occlusionindex = 0x2b;
            constexpr uint32_t buseocclusionquery = 0x20000000;

            // Layout confirmado no dump, lendo o proprio loop do jogo que
            // preenche o array (sub_7FF7F1DEFBC4):
            //
            //   v0 = &array;
            //   do {
            //     if ((*((BYTE*)v0 + 124) & 1) != 0) {   // +0x7C bit0 = entry ativa
            //       v3 = -1;                              // SEM RESULTADO = -1
            //       if (queryOk) v3 = pixels;
            //       *((DWORD*)v0 + 30) = v3;              // +0x78 = contador
            //     }
            //     v0 += 16;                               // QWORD* += 16 = 0x80
            //   } while (v0 < endPtr);
            //
            constexpr int entrysize = 0x80;
            constexpr int pixelcount = 0x78;
            constexpr int activeflag = 0x7C;

            // (endPtr - base) = 0x2200  ->  0x2200 / 0x80 = 68 entries.
            // O jogo inteiro tem 68 slots de occlusion query.
            constexpr int maxentries = 68;
        }

        namespace boneids {
            constexpr unsigned int head = 0x796e;
            constexpr unsigned int spine = 0x60f2;
            constexpr unsigned int neck = 0x9995;
            constexpr unsigned int l_foot = 0xcc;
            constexpr unsigned int r_foot = 0x9000;
            constexpr unsigned int l_thigh = 0xe39f;
            constexpr unsigned int r_thigh = 0xca72;
            constexpr unsigned int pelvis = 0x2e28;
        }

        class cvisibilitychecker {
        private:
            using clock = std::chrono::steady_clock;

            struct visibilityentry {
                bool isvisible = false;
                clock::time_point timestamp{};
                // Quando veio a ultima resposta REAL de raycast. Separado do
                // timestamp porque aquele e renovado a cada consulta, inclusive
                // quando estamos so segurando um estado antigo -- usa-lo para
                // medir validade fazia o estado nunca envelhecer.
                clock::time_point lastreal{};
                int consecutivevisible = 0;
                int consecutivenotvisible = 0;
            };

            struct losentry {
                int  value = -1;          // estado confirmado: -1 desconhecido, 0 bloqueado, 1 livre
                int  raw = -1;            // ultima leitura crua
                int  agree = 0;           // leituras seguidas que concordam com raw
                int  sweep = 0;           // contador da varredura completa
                clock::time_point stamp{};
            };

            uintptr_t m_occlusionqueryarray = 0;
            std::unordered_map<uintptr_t, visibilityentry> m_visibilitycache;
            std::unordered_map<uintptr_t, losentry> m_loscache;
            std::unordered_set<uintptr_t> m_occlusionenabled;
            std::mutex m_cachemutex;
            bool m_initialized = false;
            int m_framessincecleanup = 0;

            // O raycast e caro (ida e volta ate a thread do jogo), entao cada
            // passada da EntityList so gasta um orcamento pequeno e o resto
            // reaproveita o cache. Assim varremos todo mundo em poucas voltas
            // sem segurar a thread do ESP.
            uintptr_t m_localplayer = 0;
            uintptr_t m_pendingentity = 0;   // alvo do pedido de raycast em voo

            // Rodizio. So cabe UM pedido em voo, e antes a primeira entidade
            // vencida do loop pegava o canal -- como o refresh de quem esta
            // visivel e 8ms e uma passada leva ~1-2ms, ela vencia de novo
            // antes das outras terem vez e monopolizava o canal. O resto
            // nunca era lido e ficava vermelho para sempre.
            // Agora a passada elege a entidade que espera HA MAIS TEMPO.
            // Fila de candidatas da passada, processada em lote no
            // updateframe. Uma consulta por passada dava ~60ms entre leituras
            // do mesmo ped, e era latencia demais para o ESP acompanhar.
            std::vector<std::pair<uintptr_t, double>> m_candidates;
            // Dimensionado junto com o spin: ~15 voltas de ~10us por consulta
            // da ~150us por ped, entao 10 peds custam ~1.5ms de passada. Isso
            // rende ~6000 leituras/s -- folga larga para os ~30 peds de uma rua
            // cheia serem reavaliados a cada 25ms.
            static constexpr int los_batch_per_pass = 12;

            // Voltas de Collect antes de desistir de uma consulta. Precisa
            // cobrir o tempo do shape test do jogo, nao so a syscall.
            static constexpr int los_spin_limit = 40;

            // De quantas em quantas leituras um ped bloqueado leva a varredura
            // completa dos tres pontos, em vez de so o peito.
            static constexpr int los_full_sweep_every = 4;
            int  m_losbudget = 0;

            // Medicao da passada. Estimar isso de cabeca me levou a otimizar
            // a coisa errada mais de uma vez.
            int    m_passes = 0;          // passadas desde a ultima amostra
            int    m_lastCand = 0;        // peds concorrendo na ultima passada
            int    m_lastApplied = 0;     // leituras concluidas na ultima passada
            float  m_passHz = 0.f;        // passadas por segundo, medido
            clock::time_point m_rateStamp{};

            // LIGADO. Foi desligado uma vez por engano: o crash da epoca era
            // o use-after-free do EntityList (dois leitores iteravam o vetor
            // enquanto a thread de fundo fazia std::move), nao o raycast --
            // tanto que o crash continuou depois de desliga-lo aqui.
            //
            // Sem o raycast sobra so a occlusion query, e o jogo mantem 68
            // slots de query no total: quase todo ped fica sem dado e o check
            // marca todo mundo como ocluido.
            bool m_losenabled = true;

            static constexpr int    los_budget_per_pass = 12;

            // Assimetrico de proposito:
            //
            // Quem esta VISIVEL e reconferido quase toda passada e NAO gasta
            // orcamento -- e isso que faz o alvo sumir no mesmo instante em que
            // entra atras da parede. 8ms e meio frame a 60fps.
            //
            // Quem esta ESCONDIDO so precisa reaparecer sem atraso perceptivel,
            // e o intervalo cresce com a distancia: o orcamento vai para quem
            // esta perto, que e quem voce pode de fato acertar.
            static constexpr double los_refresh_visible_ms = 14.0;
            // 8ms era mais fino que o olho e que o proprio frame: gastava o
            // orcamento inteiro reconferindo quem ja estava visivel, deixando
            // o resto sem leitura. 25ms e ~1.5 frame a 60fps -- continua
            // instantaneo e sobra vazao para todo mundo ser alcancado.

            // Leituras seguidas que precisam concordar para trocar de estado.
            // 2 x 8ms = 16ms para sumir: continua instantaneo aos olhos, mas
            // um probe solto que raspou numa quina nao pisca mais a cor.
            static constexpr int los_confirm_samples = 2;

            // Acima disso a resposta e velha demais para valer qualquer coisa:
            // vira "nao sei", e "nao sei" e tratado conservadoramente.
            static constexpr double los_hard_stale_ms = 350.0;

            // Teto para segurar um estado sem resposta nova. Passou disso,
            // assume ocluido em vez de continuar afirmando o que era verdade
            // ha meio segundo.
            static constexpr double los_hold_max_ms = 450.0;

            // Toda transicao custa: esperar a proxima leitura, mais uma
            // leitura para a histerese confirmar. Entao o intervalo aqui e
            // METADE da latencia percebida para alguem sair da cobertura.
            //
            // Os 150ms de longe faziam um alvo distante levar ~300ms para
            // aparecer. Como o lote rende ~2200 leituras/s e uma rua cheia
            // precisa de ~1200, havia folga para apertar.
            static double los_refresh_hidden_ms(float dist) {
                if (dist < 100.0f) return 18.0;
                if (dist < 300.0f) return 35.0;
                return 70.0;
            }

            static constexpr int    cache_cleanup_interval = 60;
            static constexpr double cache_expire_ms = 2.0;
            static constexpr double entry_stale_ms = 500.0;

            // O raycast e exato em qualquer distancia; 500m cortava alvo de
            // sniper a toa. O custo de alcance e controlado pelo intervalo de
            // refresh acima, nao por um corte seco.
            static constexpr float  max_check_distance = 1000.0f;

            // Aparece rapido, some devagar: as occlusion queries chegam com
            // 1-2 frames de atraso, entao exigir varios "sumiu" seguidos e o
            // que impede o ESP de piscar sem deixar o alvo grudado.
            static constexpr int frames_to_appear = 1;
            static constexpr int frames_to_vanish = 3;

            // Quantos pixels contam como "da pra ver".
            //
            // A area do ped na tela cai com o QUADRADO da distancia. A versao
            // antiga fazia o contrario: exigia 5 pixels colado e 100 pixels a
            // 500m. Como um player a 300m ocupa ~4 pixels na tela, ele era
            // marcado como invisivel SEMPRE -- era esse o bug principal.
            static int pixelthreshold(float distance) {
                if (distance <= 1.0f) return 1;

                // area aproximada do corpo em pixels (1080p, fov ~60)
                const float expected = 400000.0f / (distance * distance);

                int t = static_cast<int>(expected * 0.02f); // 2% do corpo visivel
                if (t < 1)  t = 1;
                if (t > 32) t = 32;
                return t;
            }

            // O array e estatico, entao tem de cair dentro da imagem do modulo.
            bool plausiblearray(uintptr_t addr) const {
                if (!addr) return false;
                if (!Core::Mem.ModBase || !Core::Mem.ModBaseSize) return false;
                return addr >= Core::Mem.ModBase
                    && addr < Core::Mem.ModBase + Core::Mem.ModBaseSize;
            }

        private:
            // -1 = sem resposta util (cai para a occlusion query)
            int queryLos(uintptr_t entityaddr, float distance, clock::time_point now) {
                if (!m_losenabled || !m_localplayer || entityaddr == m_localplayer)
                    return -1;

                bool WasVisible = false;
                bool Known = false;
                int  Cached = -1;
                clock::time_point CachedStamp{};
                {
                    std::lock_guard<std::mutex> lock(m_cachemutex);
                    auto it = m_loscache.find(entityaddr);
                    if (it != m_loscache.end() && it->second.value >= 0) {
                        Known = true;
                        Cached = it->second.value;
                        CachedStamp = it->second.stamp;
                        WasVisible = (Cached == 1);

                        const double agems =
                            std::chrono::duration<double, std::milli>(now - it->second.stamp).count();
                        const double refresh = WasVisible
                            ? los_refresh_visible_ms
                            : los_refresh_hidden_ms(distance);

                        if (agems < refresh)
                            return Cached;
                    }
                }

                // NUNCA misturar os dois oraculos. Se ja temos uma resposta de
                // raycast para essa entidade, devolvemos a ULTIMA resposta de
                // raycast -- cair na occlusion query aqui fazia o estado
                // alternar entre duas fontes que discordam, e era isso que
                // piscava na tela.

                // Nao desliga por falha: o Initialize tem backoff proprio.
                // Desligar aqui
                // na primeira falha matava o raycast pela sessao inteira --
                // e a primeira tentativa costuma acontecer antes do codigo do
                // jogo estar mapeado.
                if (!Core::Features::g_LosProbe.Available()) {
                    if (!Core::Features::g_LosProbe.Initialize())
                        return -1;
                }

                // O pedido e ASSINCRONO. A versao anterior ficava esperando a
                // resposta, e cada volta do polling e uma syscall
                // (ZwReadVirtualMemory): com varias consultas por passada isso
                // segurava a thread do EntityList por milissegundos, e era dai
                // que vinha o atraso do ESP inteiro.
                //
                // Agora posta e sai; a resposta e recolhida numa passada
                // seguinte, com o mesmo tratamento de histerese de antes.
                // Nao posta aqui: apenas concorre. Quem posta e o updateframe,
                // escolhendo a entidade mais atrasada da passada inteira.
                {
                    double Age = 1e9;   // nunca lida = prioridade maxima
                    if (Known) {
                        std::lock_guard<std::mutex> lock(m_cachemutex);
                        auto it = m_loscache.find(entityaddr);
                        if (it != m_loscache.end())
                            Age = std::chrono::duration<double, std::milli>(now - it->second.stamp).count();
                    }

                    if (m_candidates.size() < 128)
                        m_candidates.emplace_back(entityaddr, Age);
                }

                // Limite duro de validade. "Known ? Cached : -1" servia um
                // valor vencido indefinidamente enquanto o lote nao alcancasse
                // aquela entidade -- um "visivel" de 2 segundos atras continuava
                // sendo respondido como visivel. Era a maior fonte de falso
                // positivo que sobrava.
                if (!Known) return -1;

                const double AgeMs =
                    std::chrono::duration<double, std::milli>(now - CachedStamp).count();

                return (AgeMs <= los_hard_stale_ms) ? Cached : -1;
            }

            // Processa um lote de consultas por passada.
            //
            // O worker fica em espera ativa (PAUSE) enquanto ha trabalho, entao
            // responde em microssegundos. Uma espera curta e limitada por
            // consulta rende varias leituras por passada em vez de uma -- e a
            // latencia entre leituras do mesmo ped e o que mais gerava falso
            // positivo, nao a precisao do raycast em si.
            void runLosBatch(clock::time_point now) {
                if (!m_losenabled || !m_localplayer || m_candidates.empty()) {
                    m_candidates.clear();
                    return;
                }

                if (!Core::Features::g_LosProbe.Available()) {
                    if (!Core::Features::g_LosProbe.Initialize()) {
                        m_candidates.clear();
                        return;
                    }
                }

                // Mais atrasadas primeiro: quem nunca foi lido tem idade 1e9.
                std::sort(m_candidates.begin(), m_candidates.end(),
                    [](const auto& A, const auto& B) { return A.second > B.second; });

                m_lastCand = (int)m_candidates.size();
                m_lastApplied = 0;

                const int N = (int)std::min<size_t>(m_candidates.size(), los_batch_per_pass);

                // Pontos testados no alvo. O probe chega em targetZ+0.75+offset.
                //
                // Todos ficam DENTRO do envelope do torso em qualquer pose.
                // Isso importa: o probe testa se a linha ate um PONTO NO ESPACO
                // esta livre, e o ped e excluido da colisao. Um offset que
                // ultrapasse o corpo (ex: +0.60 num ped agachado ou dentro de
                // carro, que da targetZ+1.35, acima da cabeca dele) atravessa
                // ar vazio, volta "livre", e marca como visivel alguem
                // totalmente atras de cobertura.
                //
                //   +0.35  parte alta do torso
                //    0.00  peito
                //   -0.35  parte baixa do torso
                static const float Points[] = { 0.00f, 0.35f, -0.35f };

                for (int i = 0; i < N; i++) {
                    const uintptr_t Target = m_candidates[i].first;

                    // Quantos pontos vale a pena testar nesta leitura.
                    //
                    // Medicao: 85% das consultas voltam bloqueadas, e uma
                    // entidade bloqueada gasta os TRES pontos (o laco so para
                    // no primeiro livre). Com ~12 syscalls por consulta, era
                    // isso que dominava o tempo da passada.
                    //
                    // Mas um ped ja tido como bloqueado, cujo peito continua
                    // bloqueado, nao mudou de estado -- varrer o resto do corpo
                    // nao muda a resposta. A varredura completa fica para quem
                    // pode MUDAR: quem esta visivel, quem e desconhecido, e uma
                    // reconferencia periodica dos bloqueados, para nao perder
                    // alguem que saiu da cobertura so pela cabeca.
                    // A ABI Steam nao aceita deslocamento no destino. Nela,
                    // repetir os tres pontos consultaria a mesma linha.
                    int PointsToTry = Core::Features::g_LosProbe.SupportsOffset() ? 3 : 1;
                    {
                        std::lock_guard<std::mutex> lock(m_cachemutex);
                        auto it = m_loscache.find(Target);
                        if (PointsToTry > 1 && it != m_loscache.end() && it->second.value == 0) {
                            if (++it->second.sweep >= los_full_sweep_every)
                                it->second.sweep = 0;      // toca a varredura completa
                            else
                                PointsToTry = 1;
                        }
                    }

                    int  Result = 0;
                    bool Complete = true;   // testamos todos os pontos previstos?
                    int  Tried = 0;

                    for (float Off : Points) {
                        if (Tried++ >= PointsToTry) break;
                        if (Core::Features::g_LosProbe.Busy() ||
                            !Core::Features::g_LosProbe.Post(m_localplayer, Target, Off))
                        {
                            Complete = false;   // canal ocupado: nao concluimos
                            break;
                        }

                        m_pendingentity = Target;

                        // O worker faz um shape test SINCRONO contra o mundo
                        // fisico -- isso leva bem mais que uma syscall. Com o
                        // limite em 6 voltas quase toda consulta estourava,
                        // marcava a entidade como incompleta e abandonava o
                        // lote inteiro da passada. Quase ninguem era lido, todos
                        // estouravam o prazo do hold ao mesmo tempo e a tela
                        // inteira alternava entre vermelho e branco.
                        int r = -1;
                        for (int spin = 0; spin < los_spin_limit && r < 0; spin++)
                            r = Core::Features::g_LosProbe.Collect();

                        if (r < 0) { Complete = false; break; }

                        m_pendingentity = 0;

                        if (r == 1) { Result = 1; break; }   // um ponto livre basta
                    }

                    // So grava "bloqueado" se realmente testou o corpo inteiro.
                    // Antes, sair no meio (canal ocupado, sem resposta a tempo)
                    // registrava 0 como se todos os pontos tivessem falhado --
                    // e isso ainda envenenava a histerese.
                    if (Result == 1 || Complete) {
                        applyLos(Target, Result, now);
                        m_lastApplied++;
                    }

                    if (!Complete) break;   // canal saturado: resto fica para a proxima
                }

                m_candidates.clear();
            }

            // Recolhe a resposta em voo, se chegou, e aplica a histerese.
            // Chamado uma vez por passada, no updateframe.
            void collectLos(clock::time_point now) {
                if (!m_losenabled) return;

                // SEMPRE drena, mesmo sem dono. Se o lote saiu no meio de uma
                // entidade, o pedido ficava em voo sem ninguem para recolher e
                // o canal travava ate o timeout de 50ms -- perdendo dezenas de
                // consultas por segundo.
                const int r = Core::Features::g_LosProbe.Collect();
                if (r < 0) return;

                if (!m_pendingentity) return;   // resposta orfa: descarta

                applyLos(m_pendingentity, r, now);
                m_pendingentity = 0;
            }

            void applyLos(uintptr_t entity, int r, clock::time_point now) {
                std::lock_guard<std::mutex> lock(m_cachemutex);
                auto& e = m_loscache[entity];

                if (r == e.raw) e.agree++;
                else { e.raw = r; e.agree = 1; }

                // Histerese SIMETRICA: qualquer troca de estado exige
                // confirmacao, nos dois sentidos.
                //
                // Ja foi assimetrica -- escondia na primeira leitura bloqueada
                // e so voltava com duas livres. Aquilo era remendo para a epoca
                // em que so cabia um raycast em voo e as leituras ficavam a
                // ~60ms uma da outra. Com o lote elas estao muito mais
                // proximas, e a assimetria passou a ser o problema: um
                // \"bloqueado\" isolado (alguem da multidao cruzando o probe, o
                // braco da animacao passando na frente) escondia o alvo na hora
                // e custava mais duas leituras para voltar. Cada ruido virava
                // tres leituras de sumico, e era isso que piscava.
                //
                // Exigindo confirmacao dos dois lados, um ruido isolado nao
                // muda nada, e uma mudanca real ainda entra em duas leituras.
                if (e.value < 0)
                    e.value = r;                                  // primeira leitura
                else if (r != e.value && e.agree >= los_confirm_samples)
                    e.value = r;                                  // troca confirmada

                e.stamp = now;
            }

        public:
            // Chamado pela EntityList: o raycast precisa saber a origem.
            void setlocalplayer(uintptr_t p) { m_localplayer = p; }
            bool losactive() const { return m_losenabled && Core::Features::g_LosProbe.Available(); }

            struct PassStats { float Hz; int Cand; int Applied; };
            PassStats passstats() const { return { m_passHz, m_lastCand, m_lastApplied }; }

            bool initialize() {
                if (m_initialized) return true;

                // Esta funcao varre o MODULO INTEIRO atras da assinatura, e ela
                // e chamada do loop da lista de entidades, que roda a cada 2ms.
                // Sem freio, uma assinatura que nao resolve nesta build virava
                // centenas de varreduras de modulo por segundo -- e cada uma
                // trava o jogo por um tempo bem visivel.
                //
                // Espacamento curto nas primeiras tentativas (o modulo pode
                // ainda estar carregando) e depois bem longo, para o caso de
                // simplesmente nao existir aqui.
                static int Fails = 0;
                static std::chrono::steady_clock::time_point NextTry{};

                const auto Now = std::chrono::steady_clock::now();

                if (Now < NextTry)
                    return false;

                NextTry = Now + std::chrono::milliseconds(Fails < 5 ? 500 : 30000);
                ++Fails;

                // "48 8D 1D ?? ?? ?? ?? FF C1" = lea rbx,[rip+X] ; inc ecx
                //
                // O fallback antigo era so "48 8D 1D ?? ?? ?? ??" -- um
                // lea rbx,[rip+X] qualquer, que aparece aos MILHARES no
                // modulo. FindSignature devolve o primeiro, entao o array
                // apontava para memoria aleatoria e todo pixel lido era lixo.
                // Sem fallback: melhor falhar limpo do que ler lixo.
                uintptr_t patternaddr = Core::Mem.FindSignature(
                    { 0x48, 0x8D, 0x1D, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xC1 });

                if (!patternaddr) return false;

                int32_t rel = Core::Mem.Read<int32_t>(patternaddr + 3);
                uintptr_t addr = patternaddr + 7 + rel;

                if (!plausiblearray(addr)) return false;

                m_occlusionqueryarray = addr;
                m_initialized = true;
                return true;
            }

            bool isinitialized() const { return m_initialized; }

            bool enableocclusionquery(uintptr_t entityaddr) {
                if (!entityaddr) return false;
                if (m_occlusionenabled.count(entityaddr)) return true;

                uint32_t flags = Core::Mem.Read<uint32_t>(entityaddr + vischeckoffsets::entityflags);
                if (!(flags & vischeckoffsets::buseocclusionquery)) {
                    flags |= vischeckoffsets::buseocclusionquery;
                    Core::Mem.Write<uint32_t>(entityaddr + vischeckoffsets::entityflags, flags);
                }

                m_occlusionenabled.insert(entityaddr);
                return true;
            }

            // >= 0 : contagem de pixels da query
            // -1   : sem dado (nenhuma query atribuida a essa entidade)
            //
            // A conta e a MESMA que CPed::IsVisible() usa:
            //     (query_id - 1) * 0x80 + 0x78
            //
            // A versao antiga fazia "idx*128 - 8" (que da no mesmo endereco, ok)
            // e, quando o valor nao servia, caia para "idx*128 + 120" -- que e
            // (idx)*128 + 0x78, ou seja, a entry SEGUINTE: a contagem de pixels
            // de OUTRA entidade. Esse fallback foi removido.
            int checkpixels(uintptr_t entityaddr) {
                if (!m_initialized || !entityaddr || !m_occlusionqueryarray) return -1;

                uintptr_t visualptr = Core::Mem.Read<uintptr_t>(entityaddr + vischeckoffsets::visualptr);
                if (!visualptr) return -1;

                uint8_t idx = Core::Mem.Read<uint8_t>(visualptr + vischeckoffsets::occlusionindex);

                // idx e 1-based; 0 = sem slot. Acima de 68 seria leitura fora
                // do array (o indice cabe num byte, mas o array nao tem tudo isso).
                if (idx < 1 || idx > vischeckoffsets::maxentries) return -1;

                const uintptr_t slot = m_occlusionqueryarray
                    + (static_cast<uintptr_t>(idx - 1) * vischeckoffsets::entrysize);

                // O jogo so escreve o contador quando esse bit esta ligado.
                // Sem ele, o valor em +0x78 e sobra da rodada anterior.
                uint8_t active = Core::Mem.Read<uint8_t>(slot + vischeckoffsets::activeflag);
                if (!(active & 1)) return -1;

                int32_t px = Core::Mem.Read<int32_t>(slot + vischeckoffsets::pixelcount);
                if (px < 0 || px > 50000) return -1;   // -1 = query ainda sem resultado

                return px;
            }


            bool isentityvisiblefast(uintptr_t entityaddr, float distance = 0.0f) {
                if (!entityaddr)
                    return false;

                if (distance > max_check_distance)
                    return false;

                const auto now = clock::now();

                {
                    std::lock_guard<std::mutex> lock(m_cachemutex);
                    auto it = m_visibilitycache.find(entityaddr);
                    if (it != m_visibilitycache.end()) {
                        double agems = std::chrono::duration<double, std::milli>(now - it->second.timestamp).count();
                        if (agems < cache_expire_ms)
                            return it->second.isvisible;
                    }
                }

                // ---- caminho principal: linha de visao real do proprio jogo ----
                const int los = queryLos(entityaddr, distance, now);
                if (los >= 0) {
                    std::lock_guard<std::mutex> lock(m_cachemutex);
                    auto& e = m_visibilitycache[entityaddr];
                    e.isvisible = (los == 1);
                    e.timestamp = now;
                    e.lastreal = now;
                    return e.isvisible;
                }

                // Com o raycast ativo ele e a UNICA fonte. Enquanto ele ainda
                // nao respondeu para esta entidade, seguramos o ultimo estado
                // em vez de consultar a occlusion query: as duas discordam, e
                // alternar entre elas era o que piscava a cor na tela.
                if (m_losenabled && Core::Features::g_LosProbe.Available()) {
                    std::lock_guard<std::mutex> lock(m_cachemutex);
                    auto it = m_visibilitycache.find(entityaddr);
                    if (it != m_visibilitycache.end()) {
                        // Segurar tem prazo. Sem isto o estado antigo era
                        // devolvido para sempre E o timestamp era renovado a
                        // cada consulta, entao a entrada nunca envelhecia nem
                        // era limpa -- um "visivel" podia sobreviver enquanto o
                        // lote nao alcancasse aquele ped.
                        const double HeldMs = std::chrono::duration<double, std::milli>(
                            now - it->second.lastreal).count();

                        if (HeldMs > los_hold_max_ms)
                            it->second.isvisible = false;

                        it->second.timestamp = now;
                        return it->second.isvisible;
                    }

                    // Sem resposta de raycast para essa entidade: CONSERVADOR.
                    //
                    // Isso ja foi "assumir visivel", o que fazia sentido quando
                    // o canal estava monopolizado e quase ninguem era lido. Com
                    // o rodizio e o lote, cada ped e alcancado em uma ou duas
                    // passadas -- e o jogo faz peds entrarem e sairem o tempo
                    // todo, entao sempre havia uma leva recem-aparecida pintada
                    // de visivel sem nunca ter sido testada. Era falso positivo
                    // constante e renovado.
                    auto& e = m_visibilitycache[entityaddr];
                    e.isvisible = false;
                    e.timestamp = now;
                    return false;
                }

                // ---- reserva: occlusion query (so quando o raycast nao existe) ----
                // A consulta LOS acima tambem tenta inicializar o probe. Nao
                // podemos sair antes dela quando o array de oclusao variar
                // entre builds. Sem nenhum dos dois, visibilidade e desconhecida.
                if (!m_initialized || !m_occlusionqueryarray)
                    return false;
                enableocclusionquery(entityaddr);

                const int pixels = checkpixels(entityaddr);

                // Existem 68 slots de query para o jogo INTEIRO, distribuidos
                // pelo renderer. A maioria dos peds nunca recebe um, entao
                // "sem dado" e o caso comum, nao a excecao -- e nenhuma das
                // duas respostas possiveis esta certa:
                //   "sem dado = invisivel" esconde quem esta em campo aberto;
                //   "sem dado = visivel"   pinta todo mundo atras da parede.
                // Mantemos o ultimo estado conhecido e, sem nenhum, ficamos no
                // conservador. A correcao de verdade e o raycast (LosProbe).
                if (pixels < 0) {
                    std::lock_guard<std::mutex> lock(m_cachemutex);
                    auto it = m_visibilitycache.find(entityaddr);
                    if (it != m_visibilitycache.end()) {
                        // Segurar tem prazo. Sem isto o estado antigo era
                        // devolvido para sempre E o timestamp era renovado a
                        // cada consulta, entao a entrada nunca envelhecia nem
                        // era limpa -- um "visivel" podia sobreviver enquanto o
                        // lote nao alcancasse aquele ped.
                        const double HeldMs = std::chrono::duration<double, std::milli>(
                            now - it->second.lastreal).count();

                        if (HeldMs > los_hold_max_ms)
                            it->second.isvisible = false;

                        it->second.timestamp = now;
                        return it->second.isvisible;
                    }
                    auto& e = m_visibilitycache[entityaddr];
                    e.isvisible = false;
                    e.timestamp = now;
                    return false;
                }

                const bool raw = (pixels >= pixelthreshold(distance));

                {
                    std::lock_guard<std::mutex> lock(m_cachemutex);
                    auto& e = m_visibilitycache[entityaddr];

                    if (raw) {
                        e.consecutivevisible++;
                        e.consecutivenotvisible = 0;
                    }
                    else {
                        e.consecutivenotvisible++;
                        e.consecutivevisible = 0;
                    }

                    if (!e.isvisible && e.consecutivevisible >= frames_to_appear)
                        e.isvisible = true;
                    else if (e.isvisible && e.consecutivenotvisible >= frames_to_vanish)
                        e.isvisible = false;

                    e.timestamp = now;

                    // A occlusion query tambem e resposta real. Sem marcar isto
                    // aqui, o prazo do hold logo acima mediria contra a epoch e
                    // travaria tudo em "ocluido" quando o raycast nao existisse.
                    e.lastreal = now;

                    return e.isvisible;
                }
            }

            bool isentityvisible(uintptr_t entityaddr, float distance = 0.0f) {
                return isentityvisiblefast(entityaddr, distance);
            }

            bool getcachedvisibility(uintptr_t entityaddr) {
                std::lock_guard<std::mutex> lock(m_cachemutex);
                auto it = m_visibilitycache.find(entityaddr);
                return (it != m_visibilitycache.end()) ? it->second.isvisible : true;
            }

            void updateframe() {
                const auto Now = clock::now();

                if (m_rateStamp.time_since_epoch().count() == 0) m_rateStamp = Now;
                m_passes++;
                {
                    const double El = std::chrono::duration<double>(Now - m_rateStamp).count();
                    if (El >= 0.5) {
                        m_passHz = (float)(m_passes / El);
                        m_passes = 0;
                        m_rateStamp = Now;
                    }
                }

                collectLos(Now);
                runLosBatch(Now);
                m_losbudget = los_budget_per_pass;   // recarrega o orcamento de raycast
                if (++m_framessincecleanup < cache_cleanup_interval) return;
                m_framessincecleanup = 0;
                cleanupoldentries();
            }

            void cleanupoldentries() {
                const auto now = clock::now();
                std::lock_guard<std::mutex> lock(m_cachemutex);

                for (auto it = m_visibilitycache.begin(); it != m_visibilitycache.end(); ) {
                    double agems = std::chrono::duration<double, std::milli>(now - it->second.timestamp).count();
                    it = (agems > entry_stale_ms) ? m_visibilitycache.erase(it) : std::next(it);
                }

                for (auto it = m_loscache.begin(); it != m_loscache.end(); ) {
                    double agems = std::chrono::duration<double, std::milli>(now - it->second.stamp).count();
                    it = (agems > entry_stale_ms) ? m_loscache.erase(it) : std::next(it);
                }

                if (m_occlusionenabled.size() > 1000)
                    m_occlusionenabled.clear();
            }

            void clearcache() {
                std::lock_guard<std::mutex> lock(m_cachemutex);
                m_visibilitycache.clear();
                m_occlusionenabled.clear();
            }
        };

        inline cvisibilitychecker g_visibilitychecker;
    }
}
