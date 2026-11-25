#pragma once
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sddl.h>
#include <string>
#include <vector>
#include <Security/xorstr.hpp>
#include <Security/Api/json.hpp>
#include <Globals.hpp>

#include <winhttp.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Normaliz.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Wldap32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "winhttp.lib")

// Autenticacao e configuracao: apenas API do Weed.
// WinHTTP faz parte do Windows, entao nao entra MSVCP/VCRUNTIME no binario.
class c_api {
private:
    using json = nlohmann::json;

    struct UrlParts {
        std::wstring Host;
        std::wstring Path;
        INTERNET_PORT Port = INTERNET_DEFAULT_HTTP_PORT;
        bool Secure = false;
    };

    struct HttpResult {
        bool Ok = false;
        long Status = 0;
        std::string Body;
        bool SslFailure = false;
    };

public:
    struct CloudConfig {
        std::string Name;
        std::string Config;
        bool Visible = false;
    };

private:
    static std::wstring ToWide( const std::string & Value ) {
        if ( Value.empty( ) )
            return { };

        const int Length = MultiByteToWideChar( CP_UTF8, 0, Value.data( ),
            static_cast<int>( Value.size( ) ), nullptr, 0 );
        if ( Length <= 0 )
            return { };

        std::wstring Result( static_cast<size_t>( Length ), L'\0' );
        MultiByteToWideChar( CP_UTF8, 0, Value.data( ), static_cast<int>( Value.size( ) ),
            Result.data( ), Length );
        return Result;
    }

    static bool ParseUrl( const std::string & Url, UrlParts & Out ) {
        const size_t SchemeEnd = Url.find( "://" );
        if ( SchemeEnd == std::string::npos )
            return false;

        const std::string Scheme = Url.substr( 0, SchemeEnd );
        if ( Scheme == xorstr( "https" ) )
            Out.Secure = true;
        else if ( Scheme == xorstr( "http" ) )
            Out.Secure = false;
        else
            return false;

        const size_t AuthorityStart = SchemeEnd + 3;
        size_t PathStart = Url.find( '/', AuthorityStart );
        if ( PathStart == std::string::npos )
            PathStart = Url.size( );

        std::string Authority = Url.substr( AuthorityStart, PathStart - AuthorityStart );
        Out.Port = Out.Secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

        const size_t Colon = Authority.rfind( ':' );
        if ( Colon != std::string::npos ) {
            const std::string PortText = Authority.substr( Colon + 1 );
            if ( !PortText.empty( ) ) {
                char * End = nullptr;
                const unsigned long Parsed = std::strtoul( PortText.c_str( ), &End, 10 );
                if ( End && *End == '\0' && Parsed > 0 && Parsed <= 65535 )
                    Out.Port = static_cast<INTERNET_PORT>( Parsed );
            }
            Authority.resize( Colon );
        }

        if ( Authority.empty( ) )
            return false;

        Out.Host = ToWide( Authority );
        const std::string Path = PathStart == Url.size( ) ? std::string( xorstr( "/" ) )
                                                          : Url.substr( PathStart );
        Out.Path = ToWide( Path );
        return !Out.Host.empty( ) && !Out.Path.empty( );
    }

    static bool IsSslError( DWORD Error ) {
        return Error == ERROR_WINHTTP_SECURE_FAILURE
            || Error == ERROR_WINHTTP_SECURE_CERT_DATE_INVALID
            || Error == ERROR_WINHTTP_SECURE_CERT_CN_INVALID
            || Error == ERROR_WINHTTP_SECURE_CERT_WRONG_USAGE
            || Error == ERROR_WINHTTP_SECURE_INVALID_CA
            || Error == ERROR_WINHTTP_SECURE_CERT_REV_FAILED
            || Error == ERROR_WINHTTP_SECURE_CERT_REVOKED
            || Error == ERROR_WINHTTP_SECURE_INVALID_CERT
            || Error == ERROR_WINHTTP_SECURE_CHANNEL_ERROR
            || Error == ERROR_WINHTTP_SECURE_FAILURE_PROXY;
    }

    static HttpResult HttpRequest( const std::string & Method, const std::string & Url,
        const std::string & Headers, const std::string & Body, bool VerifySSL ) {

        HttpResult Result;
        UrlParts Parts;
        if ( !ParseUrl( Url, Parts ) )
            return Result;

        HINTERNET Session = WinHttpOpen( L"Weed-Ketas/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0 );
        if ( !Session )
            return Result;

        WinHttpSetTimeouts( Session, 5000, 5000, 15000, 15000 );

        HINTERNET Connect = WinHttpConnect( Session, Parts.Host.c_str( ), Parts.Port, 0 );
        HINTERNET Request = nullptr;
        if ( Connect ) {
            const std::wstring WideMethod = ToWide( Method );
            Request = WinHttpOpenRequest( Connect, WideMethod.c_str( ), Parts.Path.c_str( ), nullptr,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                Parts.Secure ? WINHTTP_FLAG_SECURE : 0 );
        }

        auto Close = [&]( ) {
            if ( Request ) WinHttpCloseHandle( Request );
            if ( Connect ) WinHttpCloseHandle( Connect );
            if ( Session ) WinHttpCloseHandle( Session );
        };

        if ( !Request ) {
            Close( );
            return Result;
        }

        if ( Parts.Secure && !VerifySSL ) {
            DWORD Flags = SECURITY_FLAG_IGNORE_ALL_CERT_ERRORS;
            WinHttpSetOption( Request, WINHTTP_OPTION_SECURITY_FLAGS, &Flags, sizeof( Flags ) );
        }

        if ( !Headers.empty( ) ) {
            const std::wstring WideHeaders = ToWide( Headers );
            if ( WideHeaders.empty( ) || !WinHttpAddRequestHeaders( Request, WideHeaders.c_str( ),
                    static_cast<DWORD>( WideHeaders.size( ) ), WINHTTP_ADDREQ_FLAG_ADD ) ) {
                Close( );
                return Result;
            }
        }

        const LPVOID BodyData = Body.empty( ) ? WINHTTP_NO_REQUEST_DATA
                                              : const_cast<char *>( Body.data( ) );
        const DWORD BodyLength = static_cast<DWORD>( Body.size( ) );

        BOOL Ok = WinHttpSendRequest( Request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            BodyData, BodyLength, BodyLength, 0 );
        if ( Ok )
            Ok = WinHttpReceiveResponse( Request, nullptr );

        if ( !Ok ) {
            const DWORD Error = GetLastError( );
            Result.SslFailure = Parts.Secure && VerifySSL && IsSslError( Error );
            Close( );
            return Result;
        }

        DWORD StatusCode = 0;
        DWORD StatusLength = sizeof( StatusCode );
        WinHttpQueryHeaders( Request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &StatusCode, &StatusLength,
            WINHTTP_NO_HEADER_INDEX );
        Result.Status = static_cast<long>( StatusCode );

        for ( ;; ) {
            DWORD Available = 0;
            if ( !WinHttpQueryDataAvailable( Request, &Available ) ) {
                Close( );
                return Result;
            }
            if ( Available == 0 )
                break;
            if ( Result.Body.size( ) + Available > 1024 * 1024 ) {
                Close( );
                return Result;
            }

            std::string Chunk( Available, '\0' );
            DWORD Read = 0;
            if ( !WinHttpReadData( Request, Chunk.data( ), Available, &Read ) ) {
                Close( );
                return Result;
            }
            Result.Body.append( Chunk.data( ), Read );
            if ( Read == 0 )
                break;
        }

        Result.Ok = true;
        Close( );
        return Result;
    }

    // Tenta com certificado valido e repete sem validacao somente em falha de SSL.
    static HttpResult RequestRaw( const std::string & Method, const std::string & Url,
        const std::string & Headers = { }, const std::string & Body = { } ) {

        HttpResult Result = HttpRequest( Method, Url, Headers, Body, true );
        if ( !Result.Ok && Result.SslFailure )
            Result = HttpRequest( Method, Url, Headers, Body, false );
        return Result;
    }

    static bool RequestJson( const std::string & Method, const std::string & Url,
        const std::string & Headers, const std::string & Body,
        HttpResult & Transport, json & Parsed ) {

        Transport = RequestRaw( Method, Url, Headers, Body );
        if ( !Transport.Ok )
            return false;

        try {
            Parsed = json::parse( Transport.Body );
            return Parsed.is_object( );
        }
        catch ( const std::exception & ) {
            return false;
        }
    }

    // Registro "escondido" (HKCU\Environment) tem prioridade sobre o ambiente
    // do processo, que pode estar desatualizado desde que o cheat abriu.
    static std::string Setting( const char * Name, const std::string & Fallback = { } ) {
        char Buffer[1024] = { };

        HKEY Key = nullptr;
        if ( RegOpenKeyExA( HKEY_CURRENT_USER, xorstr( "Environment" ), 0, KEY_READ, &Key )
            == ERROR_SUCCESS ) {
            DWORD Type = 0;
            DWORD Size = sizeof( Buffer );
            if ( RegQueryValueExA( Key, Name, nullptr, &Type,
                    reinterpret_cast<BYTE *>( Buffer ), &Size ) == ERROR_SUCCESS
                && ( Type == REG_SZ || Type == REG_EXPAND_SZ )
                && Size > 1 && Size <= sizeof( Buffer ) && Buffer[Size - 1] == '\0' ) {
                RegCloseKey( Key );
                return std::string( Buffer, Size - 1 );
            }
            RegCloseKey( Key );
        }

        const DWORD Length = GetEnvironmentVariableA( Name, Buffer, sizeof( Buffer ) );
        if ( Length > 0 && Length < sizeof( Buffer ) )
            return std::string( Buffer, Length );

        return Fallback;
    }

    static std::string ApiBase( ) {
        std::string Url = Setting( xorstr( "WEED_API_URL" ),
            xorstr( "https://weedsolution.xyz:8000" ) );
        while ( !Url.empty( ) && Url.back( ) == '/' )
            Url.pop_back( );
        return Url;
    }

    // The auth API and the PC config API are separate HTTPS services.
    // Auth remains on :8000; configcloud.js listens on :7000.
    static std::string ConfigBase( ) {
        std::string Url = Setting( xorstr( "WEED_CONFIG_URL" ),
            xorstr( "https://weedsolution.xyz:7000" ) );
        while ( !Url.empty( ) && Url.back( ) == '/' )
            Url.pop_back( );
        return Url;
    }

    static std::vector<std::string> AuthUrls( ) {
        const std::string Configured = Setting( xorstr( "WEED_AUTH_URL" ) );
        if ( !Configured.empty( ) )
            return { Configured };

        return {
            xorstr( "https://weedsolution.xyz:8000/auth/combined-login" )
        };
    }

    static bool IsSafeHeaderValue( const std::string & Value ) {
        if ( Value.empty( ) || Value.size( ) > 256 )
            return false;

        for ( char Raw : Value ) {
            const unsigned char C = static_cast<unsigned char>( Raw );
            const bool AlphaNum = ( C >= 'a' && C <= 'z' ) || ( C >= 'A' && C <= 'Z' )
                || ( C >= '0' && C <= '9' );
            if ( !AlphaNum && C != '-' && C != '_' && C != '.' && C != '~' )
                return false;
        }
        return true;
    }

    static bool IsSafeName( const std::string & Name ) {
        if ( Name.empty( ) || Name.size( ) > 64 )
            return false;

        for ( char Raw : Name ) {
            const unsigned char C = static_cast<unsigned char>( Raw );
            if ( C < 0x20 || C == '/' || C == '\\' )
                return false;
        }
        return true;
    }

    static std::string UrlEncode( const std::string & Value ) {
        static const char Hex[] = "0123456789ABCDEF";
        std::string Encoded;
        Encoded.reserve( Value.size( ) * 3 );

        for ( char Raw : Value ) {
            const unsigned char C = static_cast<unsigned char>( Raw );
            const bool Safe = ( C >= 'a' && C <= 'z' ) || ( C >= 'A' && C <= 'Z' )
                || ( C >= '0' && C <= '9' ) || C == '-' || C == '_' || C == '.' || C == '~';
            if ( Safe ) {
                Encoded.push_back( static_cast<char>( C ) );
            }
            else {
                Encoded.push_back( '%' );
                Encoded.push_back( Hex[C >> 4] );
                Encoded.push_back( Hex[C & 0xF] );
            }
        }
        return Encoded;
    }

    static std::string JsonHeaders( ) {
        return xorstr( "Content-Type: application/json\r\n" );
    }

    std::string ApiHeaders( ) const {
        std::string Headers = JsonHeaders( );
        const std::string Key = ApiKey( );
        if ( !Key.empty( ) )
            Headers += std::string( xorstr( "x-api-key: " ) ) + Key + "\r\n";
        return Headers;
    }

    static std::string EndpointError( const HttpResult & Result ) {
        std::string Body = Result.Body;
        while ( !Body.empty( ) && ( Body.front( ) == ' ' || Body.front( ) == '\r'
            || Body.front( ) == '\n' ) )
            Body.erase( Body.begin( ) );
        while ( !Body.empty( ) && ( Body.back( ) == ' ' || Body.back( ) == '\r'
            || Body.back( ) == '\n' ) )
            Body.pop_back( );

        if ( !Body.empty( ) && Body.size( ) <= 200 )
            return Body;

        return std::string( xorstr( "HTTP " ) ) + std::to_string( Result.Status )
            + ( Result.Status == 0 ? std::string( xorstr( " (sem resposta)" ) ) : std::string( ) );
    }

    static std::string CombinedErrorMessage( const json & Response, long Status ) {
        const std::string Code = Response.value( xorstr( "error" ), std::string( ) );

        if ( Code == xorstr( "not_found" ) ) return xorstr( "Account not found." );
        if ( Code == xorstr( "banned" ) ) return xorstr( "This account is banned." );
        if ( Code == xorstr( "invalid_credentials" ) ) return xorstr( "Invalid username or password." );
        if ( Code == xorstr( "hwid_mismatch" ) ) return xorstr( "HWID does not match the registered one." );
        if ( Code == xorstr( "no_active_game" ) ) return xorstr( "No active game license on this account." );
        if ( Code == xorstr( "game_not_found" ) ) return xorstr( "No Fivem license on this account." );
        if ( Code == xorstr( "game_license_expired" ) ) return xorstr( "Fivem license expired." );
        if ( Code == xorstr( "server_error" ) ) return xorstr( "Authentication server error." );
        if ( !Code.empty( ) ) return Code;

        const std::string Message = Response.value( xorstr( "message" ), std::string( ) );
        if ( !Message.empty( ) && Message != xorstr( "login_success" ) )
            return Message;

        if ( Status != 200 )
            return std::string( xorstr( "Authentication server error (HTTP " ) )
                + std::to_string( Status ) + xorstr( ")." );

        return xorstr( "Invalid authentication server response." );
    }

    // SID da conta atual - o mesmo valor mostrado por "whoami /user".
    static std::string CurrentUserSid( ) {
        HANDLE Token = nullptr;
        if ( !OpenProcessToken( GetCurrentProcess( ), TOKEN_QUERY, &Token ) )
            return { };

        DWORD Needed = 0;
        std::vector<BYTE> Buffer( sizeof( TOKEN_USER ) + sizeof( SID ) );
        BOOL Ok = GetTokenInformation( Token, TokenUser, Buffer.data( ),
            static_cast<DWORD>( Buffer.size( ) ), &Needed );
        if ( !Ok && Needed > Buffer.size( ) ) {
            Buffer.resize( Needed );
            Ok = GetTokenInformation( Token, TokenUser, Buffer.data( ),
                static_cast<DWORD>( Buffer.size( ) ), &Needed );
        }
        CloseHandle( Token );

        if ( !Ok )
            return { };

        const auto * User = reinterpret_cast<const TOKEN_USER *>( Buffer.data( ) );
        if ( !IsValidSid( User->User.Sid ) )
            return { };

        LPSTR SidString = nullptr;
        if ( !ConvertSidToStringSidA( User->User.Sid, &SidString ) )
            return { };

        std::string Sid( SidString );
        LocalFree( SidString );
        return Sid;
    }

    void SetError( const std::string & Message ) {
        std::lock_guard<std::mutex> Guard( AuthMutex );
        AuthMessage = Message;
    }

    std::string ApiKey( ) const {
        {
            std::lock_guard<std::mutex> Guard( AuthMutex );
            if ( !AuthApiKey.empty( ) )
                return AuthApiKey;
        }
        // Chave do painel Weed (valor padrao do .env do projeto de auth).
        return Setting( xorstr( "WEED_API_KEY" ),
            Setting( xorstr( "API_SECRET_KEY" ),
                xorstr( "6cb3f03dfae5d6e65e0fb0009884ad93b2186c48ed39ab700a7a72c2522882da" ) ) );
    }

    static std::string ConfigUrl( const std::string & Username,
        const std::string & Name = { }, const std::string & Suffix = { } ) {

        std::string Url = ConfigBase( ) + xorstr( "/pc/configs/" ) + UrlEncode( Username );
        if ( !Name.empty( ) ) {
            Url += '/';
            Url += UrlEncode( Name );
            if ( !Suffix.empty( ) ) {
                Url += '/';
                Url += Suffix;
            }
        }
        return Url;
    }

    mutable std::mutex AuthMutex;
    std::atomic<bool> AuthValid{ false };
    std::string AuthToken;
    std::string AuthUser;
    std::string AuthHwid;
    std::string AuthApiKey;
    std::string AuthMessage;

public:
    ~c_api( ) { Logout( ); }

    // Login combinado do Weed: usuario + senha no painel, HWID = SID da maquina.
    bool CombinedLogin( const std::string & Username, const std::string & Password ) {
        Logout( );

        if ( Username.size( ) < 3 || Username.size( ) > 32 ) {
            SetError( xorstr( "Username must be between 3 and 32 characters." ) );
            return false;
        }
        if ( Password.size( ) < 6 || Password.size( ) > 128 ) {
            SetError( xorstr( "Password must be between 6 and 128 characters." ) );
            return false;
        }

        const std::string Hwid = CurrentUserSid( );
        if ( Hwid.empty( ) ) {
            SetError( xorstr( "Unable to identify this machine (SID)." ) );
            return false;
        }

        const json Payload = {
            { xorstr( "username" ), Username },
            { xorstr( "password" ), Password },
            { xorstr( "hwid" ), Hwid },
            { xorstr( "game" ), xorstr( "Fivem" ) }
        };
        const std::string Body = Payload.dump( );
        const std::string Headers = JsonHeaders( );

        std::string LastError;
        for ( const std::string & Url : AuthUrls( ) ) {
            HttpResult Transport;
            json Response;
            if ( !RequestJson( xorstr( "POST" ), Url, Headers, Body, Transport, Response ) ) {
                LastError = Transport.Ok
                    ? std::string( xorstr( "Invalid authentication server response." ) )
                    : EndpointError( Transport );
                continue;
            }

            std::string Token;
            for ( const char * Key : { "token", "access_token", "accessToken", "jwt", "authToken" } ) {
                const std::string Candidate = Response.value( Key, std::string( ) );
                if ( !Candidate.empty( ) ) {
                    Token = Candidate;
                    break;
                }
            }

            const std::string Message = Response.value( xorstr( "message" ), std::string( ) );
            const bool Success = Transport.Status == 200
                && ( Message == xorstr( "login_success" )
                    || Response.value( xorstr( "success" ), false )
                    || !Token.empty( ) );

            if ( !Success ) {
                LastError = CombinedErrorMessage( Response, Transport.Status );
                continue;
            }

            const std::string User = Response.value( xorstr( "username" ), Username );
            const std::string Role = Response.value( xorstr( "role" ),
                std::string( xorstr( "User" ) ) );
            const std::string KeyFromServer = Response.value( xorstr( "api_key" ),
                Response.value( xorstr( "apiKey" ), std::string( ) ) );

            {
                std::lock_guard<std::mutex> Guard( AuthMutex );
                AuthToken = Token;
                AuthHwid = Hwid;
                AuthUser = User;
                if ( IsSafeHeaderValue( KeyFromServer ) )
                    AuthApiKey = KeyFromServer;
                AuthMessage.clear( );
            }
            g_Variables.UserName = User;
            g_Variables.Role = Role;
            AuthValid = true;
            return true;
        }

        SetError( LastError.empty( )
            ? std::string( xorstr( "Unable to contact the authentication server." ) )
            : LastError );
        return false;
    }

    bool IsAuthenticated( ) const { return AuthValid.load( ); }

    std::string AuthError( ) {
        std::lock_guard<std::mutex> Guard( AuthMutex );
        return AuthMessage;
    }

    void Logout( ) {
        {
            std::lock_guard<std::mutex> Guard( AuthMutex );
            AuthToken.clear( );
            AuthUser.clear( );
            AuthHwid.clear( );
            AuthApiKey.clear( );
            AuthMessage.clear( );
        }
        AuthValid = false;
        g_Variables.UserName.clear( );
        g_Variables.Role.clear( );
    }

    // Config cloud (endpoint /pc do configcloud.js). Sem registro local.
    bool ListCloudConfigs( const std::string & Username,
        std::vector<CloudConfig> & Out, std::string & Error ) const {

        Out.clear( );
        Error.clear( );

        if ( !IsSafeName( Username ) ) {
            Error = xorstr( "Username invalido." );
            return false;
        }

        const std::string Key = ApiKey( );
        if ( !IsSafeHeaderValue( Key ) ) {
            Error = xorstr( "Configure WEED_API_KEY (HKCU\\Environment) com a chave da API." );
            return false;
        }

        HttpResult Transport;
        json Response;
        if ( !RequestJson( xorstr( "GET" ), ConfigUrl( Username ), ApiHeaders( ), { },
                Transport, Response ) ) {
            Error = Transport.Ok
                ? std::string( xorstr( "Resposta invalida da API de configs." ) )
                : EndpointError( Transport );
            return false;
        }
        if ( Transport.Status != 200 ) {
            Error = EndpointError( Transport )
                + ( Transport.Status == 401 ? std::string( xorstr( " (verifique WEED_API_KEY)" ) )
                                             : std::string( ) );
            return false;
        }
        if ( !Response.contains( xorstr( "configs" ) ) || !Response[xorstr( "configs" )].is_array( ) ) {
            Error = xorstr( "Resposta invalida da API de configs." );
            return false;
        }

        try {
            for ( const auto & Item : Response[xorstr( "configs" )] ) {
                CloudConfig Entry;
                Entry.Name = Item.value( xorstr( "configname" ), std::string( ) );
                Entry.Visible = Item.value( xorstr( "visible" ), false );

                const auto Value = Item.value( xorstr( "config" ), json( ) );
                if ( Value.is_string( ) )
                    Entry.Config = Value.get<std::string>( );
                else if ( Value.is_object( ) )
                    Entry.Config = Value.dump( );

                if ( !Entry.Name.empty( ) && !Entry.Config.empty( ) )
                    Out.push_back( std::move( Entry ) );
            }
        }
        catch ( const std::exception & ) {
            Error = xorstr( "Resposta invalida da API de configs." );
            return false;
        }
        return true;
    }

    bool CreateCloudConfig( const std::string & Username, const std::string & Name,
        const std::string & Config, bool Visible, std::string & Error ) const {

        Error.clear( );
        if ( !IsSafeName( Username ) || !IsSafeName( Name ) ) {
            Error = xorstr( "Nome de config invalido." );
            return false;
        }
        if ( Config.empty( ) ) {
            Error = xorstr( "Config vazia." );
            return false;
        }
        if ( Config.size( ) > 200 * 1024 ) {
            Error = xorstr( "Config muito grande para a API (limite 256kb)." );
            return false;
        }
        const std::string Key = ApiKey( );
        if ( !IsSafeHeaderValue( Key ) ) {
            Error = xorstr( "Configure WEED_API_KEY (HKCU\\Environment) com a chave da API." );
            return false;
        }

        const json Body = {
            { xorstr( "configname" ), Name },
            { xorstr( "config" ), Config },
            { xorstr( "visible" ), Visible }
        };
        const HttpResult Result = RequestRaw( xorstr( "POST" ), ConfigUrl( Username ),
            ApiHeaders( ), Body.dump( ) );
        if ( !Result.Ok || Result.Status != 200 ) {
            Error = EndpointError( Result );
            return false;
        }
        return true;
    }

    bool UpdateCloudConfig( const std::string & Username, const std::string & Name,
        const std::string & Config, std::string & Error ) const {

        Error.clear( );
        if ( !IsSafeName( Username ) || !IsSafeName( Name ) ) {
            Error = xorstr( "Nome de config invalido." );
            return false;
        }
        if ( Config.empty( ) || Config.size( ) > 200 * 1024 ) {
            Error = xorstr( "Config vazia ou muito grande para a API." );
            return false;
        }
        const std::string Key = ApiKey( );
        if ( !IsSafeHeaderValue( Key ) ) {
            Error = xorstr( "Configure WEED_API_KEY (HKCU\\Environment) com a chave da API." );
            return false;
        }

        const json Payload = { { xorstr( "config" ), Config } };
        const HttpResult Result = RequestRaw( xorstr( "PUT" ), ConfigUrl( Username, Name ),
            ApiHeaders( ), Payload.dump( ) );
        if ( !Result.Ok || Result.Status != 200 ) {
            Error = EndpointError( Result );
            return false;
        }
        return true;
    }

    bool RenameCloudConfig( const std::string & Username, const std::string & OldName,
        const std::string & NewName, std::string & Error ) const {

        Error.clear( );
        if ( !IsSafeName( Username ) || !IsSafeName( OldName ) || !IsSafeName( NewName ) ) {
            Error = xorstr( "Nome de config invalido." );
            return false;
        }
        const std::string Key = ApiKey( );
        if ( !IsSafeHeaderValue( Key ) ) {
            Error = xorstr( "Configure WEED_API_KEY (HKCU\\Environment) com a chave da API." );
            return false;
        }

        const json Payload = { { xorstr( "newName" ), NewName } };
        const HttpResult Result = RequestRaw( xorstr( "PUT" ),
            ConfigUrl( Username, OldName, xorstr( "rename" ) ), ApiHeaders( ), Payload.dump( ) );
        if ( !Result.Ok || Result.Status != 200 ) {
            Error = EndpointError( Result );
            return false;
        }
        return true;
    }

    bool DeleteCloudConfig( const std::string & Username, const std::string & Name,
        std::string & Error ) const {

        Error.clear( );
        if ( !IsSafeName( Username ) || !IsSafeName( Name ) ) {
            Error = xorstr( "Nome de config invalido." );
            return false;
        }
        const std::string Key = ApiKey( );
        if ( !IsSafeHeaderValue( Key ) ) {
            Error = xorstr( "Configure WEED_API_KEY (HKCU\\Environment) com a chave da API." );
            return false;
        }

        const HttpResult Result = RequestRaw( xorstr( "DELETE" ), ConfigUrl( Username, Name ),
            ApiHeaders( ), { } );
        if ( !Result.Ok || Result.Status != 200 ) {
            Error = EndpointError( Result );
            return false;
        }
        return true;
    }
};

inline c_api g_Api;
