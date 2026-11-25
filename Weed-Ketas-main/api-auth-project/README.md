# Weed config cloud

Servidor de configs do Weed (HTTPS em `https://weedsolution.xyz:8000`).

## Rodar

```powershell
cd api-auth-project
npm install
node configcloud.js
```

O `.env` do projeto ja esta configurado com `API_URL`, `PORT=8000`,
`API_SECRET_KEY`, `JWT_SECRET`, `DB_PATH=./auth_db.json` e o restante.

Certificados (conforme o codigo):

- `C:/ssl/weed/weedsolution.xyz-key.pem`
- `C:/ssl/weed/weedsolution.xyz-chain.pem`

## Arquivos

- `configcloud.js` — API (JWT para o painel, `x-api-key` para o cliente PC).
- `configcloud.json` — banco de configs.
- `auth_db.json` (`DB_PATH`) — banco de usuarios do projeto de auth; usado por `userExists` e `requireAdmin`.

## Variaveis do cliente (PC)

Lidas de `HKCU\Environment` (com fallback para variaveis de processo):

- `WEED_API_URL` — default `https://weedsolution.xyz:8000`
- `WEED_API_KEY` — default = `API_SECRET_KEY` do `.env` (ja embutido no cliente)
- `WEED_AUTH_URL` — default `https://weedsolution.xyz:8000/auth/combined-login`
