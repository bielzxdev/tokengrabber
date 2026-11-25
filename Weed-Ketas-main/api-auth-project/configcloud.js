require('dotenv').config();
const express = require('express');
const fs = require('fs');
const path = require('path');
const jwt = require('jsonwebtoken');
const https = require('https');
//const http = require('http');
const app = express();
app.use(express.json({ limit: '256kb' }));

app.use((err, req, res, next) => {
  if (err && err.type === 'entity.parse.failed') {
    return res.status(400).send('JSON inválido');
  }
  if (err && err.type === 'entity.too.large') {
    return res.status(413).send('Payload muito grande');
  }
  next(err);
});

const PORT = 7000;
const MAIN_DB = './db.json';
const CONFIG_DB = './configcloud.json';

const JWT_SECRET = process.env.JWT_SECRET;
const API_SECRET = process.env.API_SECRET_KEY;
const STAFF_ROLES = ['owner', 'developer', 'admin'];

if (!JWT_SECRET || !API_SECRET) {
  console.error('ENV inválido: configure JWT_SECRET e API_SECRET_KEY.');
  process.exit(1);
}

const SSL_KEY_PATH = 'C:/ssl/weed/weedsolution.xyz-key.pem';
const SSL_CERT_PATH = 'C:/ssl/weed/weedsolution.xyz-chain.pem';

const httpsOptions = {
    key: fs.readFileSync(SSL_KEY_PATH),
    cert: fs.readFileSync(SSL_CERT_PATH)
};

const readJson = (p) => fs.existsSync(p) ? JSON.parse(fs.readFileSync(p)) : {};

const writeJson = (p, data) => {
  const dir = path.dirname(path.resolve(p));
  const tmp = path.join(dir, `.cc_${process.pid}_${Date.now()}.tmp`);
  fs.writeFileSync(tmp, JSON.stringify(data, null, 2));
  fs.renameSync(tmp, p);
};

// AUTH MIDDLEWARES
function requireJwt(req, res, next) {
  const h = req.headers.authorization;
  if (!h || !h.startsWith('Bearer ')) return res.status(401).send('unauthorized');
  try {
    const payload = jwt.verify(h.slice(7), JWT_SECRET, { algorithms: ['HS256'] });
    req.auth = { username: payload.sub, role: payload.role };
    if (!req.auth.username) return res.status(401).send('unauthorized');
    next();
  } catch {
    return res.status(401).send('invalid_token');
  }
}

function requireApiSecret(req, res, next) {
  const k = req.headers['x-api-key'];
  if (!k || k !== API_SECRET) return res.status(401).send('unauthorized');
  next();
}

function requireAdmin(req, res, next) {
  requireJwt(req, res, () => {
    if (!STAFF_ROLES.includes(req.auth.role)) return res.status(403).send('forbidden');
    const mainDB = readJson(MAIN_DB);
    const u = (mainDB.users || []).find(x => x.username === req.auth.username);
    if (!u || !STAFF_ROLES.includes(u.role)) return res.status(403).send('forbidden');
    next();
  });
}

// SECURITY

process.on('uncaughtException', (err) => {
  console.error('Erro não tratado:', err);
});

process.on('unhandledRejection', (reason) => {
  console.error('Promessa rejeitada:', reason);
});

// if (!fs.existsSync(SSL_KEY_PATH) || !fs.existsSync(SSL_CERT_PATH)) {
//   console.error('Certificados SSL não encontrados!');
//   console.error('Verifique os caminhos:');
//   console.error(`Key:  ${SSL_KEY_PATH}`);
//   console.error(`Cert: ${SSL_CERT_PATH}`);
//   process.exit(1);
// }

// let httpsOptions;
// try {
//   httpsOptions = {
//     key: fs.readFileSync(SSL_KEY_PATH),
//     cert: fs.readFileSync(SSL_CERT_PATH)
//   };
// } catch (err) {
//   console.error('Erro ao carregar os certificados SSL:', err.message);
//   process.exit(1);
// }

const FORBIDDEN_KEYS = ['__proto__', 'constructor', 'prototype'];

function isValidString(str, maxLen = 64) {
  return typeof str === 'string' &&
    str.trim().length > 0 &&
    str.trim().length <= maxLen &&
    !FORBIDDEN_KEYS.includes(str.trim());
}

function userExists(username) {
  const mainDB = readJson(MAIN_DB);
  return !!(mainDB.users && mainDB.users.find(u => u.username === username));
}

// RATE LIMITING
const rateLimitBuckets = new Map();

function rateLimit({ windowMs = 60000, max = 30 } = {}) {
  return (req, res, next) => {
    const key = req.ip;
    const now = Date.now();
    const bucket = rateLimitBuckets.get(key) || [];
    const recent = bucket.filter(ts => now - ts < windowMs);

    if (recent.length >= max) {
      return res.status(429).send('Muitas requisições, tente novamente em instantes');
    }

    recent.push(now);
    rateLimitBuckets.set(key, recent);
    next();
  };
}

setInterval(() => {
  const now = Date.now();
  for (const [key, bucket] of rateLimitBuckets.entries()) {
    const recent = bucket.filter(ts => now - ts < 60000);
    if (recent.length === 0) rateLimitBuckets.delete(key);
    else rateLimitBuckets.set(key, recent);
  }
}, 60000).unref();

const likeLimiter = rateLimit({ windowMs: 60000, max: 30 });
const writeLimiter = rateLimit({ windowMs: 60000, max: 15 });  

// Helper
function getOrCreateUserConfig(configDB, username) {
  let uc = configDB.find(u => u.username === username);
  if (!uc) { uc = { username, configs: [] }; configDB.push(uc); }
  return uc;
}

function sanitizeDescription(desc) {
  if (desc === undefined || desc === null) return null;
  if (typeof desc !== 'string') return undefined; // undefined = inválido
  const trimmed = desc.trim();
  if (trimmed.length === 0) return null;
  if (trimmed.length > 300) return undefined;
  return trimmed;
}

// CREATE CONFIG
app.post('/createconfig', requireJwt, writeLimiter, (req, res) => {
  const username = req.auth.username;
  const { configname, config, visible } = req.body;
  if (!isValidString(configname, 48) || !config)
    return res.status(400).send('Dados incompletos ou inválidos');

  const description = sanitizeDescription(req.body.description);
  if (description === undefined) return res.status(400).send('Descrição inválida');

  if (!userExists(username)) return res.status(404).send('Usuário não encontrado');

  const configDB = readJson(CONFIG_DB) || [];
  const userConfig = getOrCreateUserConfig(configDB, username);

  if (userConfig.configs.find(c => c.configname === configname))
    return res.status(400).send('Config já existe');

  userConfig.configs.push({
    configname,
    config,
    description,
    visible: typeof visible === 'boolean' ? visible : false,
    likes: 0,
    likedBy: [],
    saves: 0
  });
  writeJson(CONFIG_DB, configDB);

  res.send('Config criada com sucesso');
});

// LOAD CONFIG
app.get('/loadconfig/:configname', requireJwt, (req, res) => {
  const username = req.auth.username;
  const { configname } = req.params;
  const configDB = readJson(CONFIG_DB) || [];

  const userConfig = configDB.find(u => u.username === username);
  if (!userConfig) return res.status(404).send('Usuário não encontrado');

  const conf = userConfig.configs.find(c => c.configname === configname);
  if (!conf) return res.status(404).send('Config não encontrada');

  res.json(conf);
});

// LIST CONFIG
app.get('/listconfigs', requireJwt, (req, res) => {
  const username = req.auth.username;
  const configDB = readJson(CONFIG_DB) || [];

  const userConfig = configDB.find(u => u.username === username);
  if (!userConfig) return res.json({ username, total: 0, configs: [] });

  res.json({
    username: userConfig.username,
    total: userConfig.configs.length,
    configs: userConfig.configs
  });
});

// LIST PUBLIC CONFIGS
app.get('/publicconfigs', requireJwt, (req, res) => {
  const configDB = readJson(CONFIG_DB) || [];
  const viewer = req.auth.username;

  const publicConfigs = [];
  configDB.forEach(userConfig => {
    userConfig.configs.forEach(c => {
      if (c.visible === true) {
        const likedBy = c.likedBy || [];
        publicConfigs.push({
          username: userConfig.username,
          configname: c.configname,
          config: c.config,
          description: c.description || null,
          likes: c.likes || 0,
          saves: c.saves || 0,
          likedByMe: likedBy.includes(viewer)
        });
      }
    });
  });

  publicConfigs.sort((a, b) => b.likes - a.likes);

  res.json({ total: publicConfigs.length, configs: publicConfigs });
});

// COPY CONFIG
app.post('/copyconfig', requireJwt, writeLimiter, (req, res) => {
  const toUsername = req.auth.username;
  const { fromUsername, configname, newConfigName } = req.body;

  if (!isValidString(fromUsername) || !isValidString(configname, 48) || !isValidString(newConfigName, 48))
    return res.status(400).send('Dados incompletos ou inválidos');

  if (fromUsername === toUsername)
    return res.status(400).send('Você não pode copiar uma config para a sua própria conta');

  const configDB = readJson(CONFIG_DB) || [];

  const sourceUserConfig = configDB.find(u => u.username === fromUsername);
  if (!sourceUserConfig) return res.status(404).send('Usuário de origem não encontrado');

  const sourceConf = sourceUserConfig.configs.find(c => c.configname === configname);
  if (!sourceConf) return res.status(404).send('Config não encontrada');

  if (!sourceConf.visible) return res.status(403).send('Essa config não é pública');

  const targetUserConfig = getOrCreateUserConfig(configDB, toUsername);

  if (targetUserConfig.configs.find(c => c.configname === newConfigName))
    return res.status(400).send('Você já possui uma config com esse nome');

  targetUserConfig.configs.push({
    configname: newConfigName,
    config: sourceConf.config,
    description: sourceConf.description || null,
    visible: false,
    likes: 0,
    likedBy: [],
    saves: 0
  });

  sourceConf.saves = (sourceConf.saves || 0) + 1;

  writeJson(CONFIG_DB, configDB);

  res.send('Config copiada com sucesso');
});

// LIKE CONFIG
app.put('/likeconfig', requireJwt, likeLimiter, (req, res) => {
  const likerUsername = req.auth.username;
  const { ownerUsername, configname } = req.body;

  if (!isValidString(ownerUsername) || !isValidString(configname, 48))
    return res.status(400).send('Dados incompletos ou inválidos');

  if (ownerUsername === likerUsername)
    return res.status(400).send('Você não pode curtir a sua própria config');

  const configDB = readJson(CONFIG_DB) || [];

  const userConfig = configDB.find(u => u.username === ownerUsername);
  if (!userConfig) return res.status(404).send('Dono da config não encontrado');

  const conf = userConfig.configs.find(c => c.configname === configname);
  if (!conf) return res.status(404).send('Config não encontrada');

  if (!conf.visible) return res.status(403).send('Essa config não é pública');

  if (!conf.likedBy) conf.likedBy = [];

  if (conf.likedBy.includes(likerUsername))
    return res.status(400).send('Você já curtiu essa config');

  conf.likedBy.push(likerUsername);
  conf.likes = conf.likedBy.length;
  writeJson(CONFIG_DB, configDB);

  res.json({ message: 'Like adicionado', likes: conf.likes });
});

// UNLIKE CONFIG
app.put('/unlikeconfig', requireJwt, likeLimiter, (req, res) => {
  const likerUsername = req.auth.username;
  const { ownerUsername, configname } = req.body;

  if (!isValidString(ownerUsername) || !isValidString(configname, 48))
    return res.status(400).send('Dados incompletos ou inválidos');

  const configDB = readJson(CONFIG_DB) || [];

  const userConfig = configDB.find(u => u.username === ownerUsername);
  if (!userConfig) return res.status(404).send('Dono da config não encontrado');

  const conf = userConfig.configs.find(c => c.configname === configname);
  if (!conf) return res.status(404).send('Config não encontrada');

  if (!conf.likedBy) conf.likedBy = [];

  const index = conf.likedBy.indexOf(likerUsername);
  if (index === -1) return res.status(400).send('Você ainda não curtiu essa config');

  conf.likedBy.splice(index, 1);
  conf.likes = conf.likedBy.length;
  writeJson(CONFIG_DB, configDB);

  res.json({ message: 'Like removido', likes: conf.likes });
});

// UPDATE VISIBILITY
app.put('/updatevisible', requireJwt, (req, res) => {
  const username = req.auth.username;
  const { configname, visible } = req.body;

  if (!isValidString(configname, 48) || typeof visible !== 'boolean')
    return res.status(400).send('Dados incompletos ou inválidos');

  const configDB = readJson(CONFIG_DB) || [];

  const userConfig = configDB.find(u => u.username === username);
  if (!userConfig) return res.status(404).send('Usuário não encontrado');

  const conf = userConfig.configs.find(c => c.configname === configname);
  if (!conf) return res.status(404).send('Config não encontrada');

  conf.visible = visible;
  writeJson(CONFIG_DB, configDB);

  res.send(`Config agora está ${visible ? 'pública' : 'privada'}`);
});

// UPDATE DESCRIPTION
app.put('/updatedescription', requireJwt, (req, res) => {
  const username = req.auth.username;
  const { configname } = req.body;

  if (!isValidString(configname, 48))
    return res.status(400).send('Dados incompletos ou inválidos');

  const description = sanitizeDescription(req.body.description);
  if (description === undefined) return res.status(400).send('Descrição inválida');

  const configDB = readJson(CONFIG_DB) || [];

  const userConfig = configDB.find(u => u.username === username);
  if (!userConfig) return res.status(404).send('Usuário não encontrado');

  const conf = userConfig.configs.find(c => c.configname === configname);
  if (!conf) return res.status(404).send('Config não encontrada');

  conf.description = description;
  writeJson(CONFIG_DB, configDB);

  res.send('Descrição atualizada com sucesso');
});

// DELETE CONFIG
app.delete('/deleteconfig', requireJwt, (req, res) => {
  const username = req.auth.username;
  const { configname } = req.body;
  if (!isValidString(configname, 48)) return res.status(400).send('Dados incompletos ou inválidos');

  const configDB = readJson(CONFIG_DB) || [];

  const userConfig = configDB.find(u => u.username === username);
  if (!userConfig) return res.status(404).send('Usuário não encontrado');

  const index = userConfig.configs.findIndex(c => c.configname === configname);
  if (index === -1) return res.status(404).send('Config não encontrada');

  userConfig.configs.splice(index, 1);
  writeJson(CONFIG_DB, configDB);

  res.send('Config deletada com sucesso');
});

// RENAME CONFIG
app.put('/renameconfig', requireJwt, (req, res) => {
  const username = req.auth.username;
  const { oldName, newName } = req.body;

  if (!isValidString(oldName, 48) || !isValidString(newName, 48))
    return res.status(400).send('Dados incompletos ou inválidos');

  const configDB = readJson(CONFIG_DB) || [];

  const userConfig = configDB.find(u => u.username === username);
  if (!userConfig) return res.status(404).send('Usuário não encontrado');

  const conf = userConfig.configs.find(c => c.configname === oldName);
  if (!conf) return res.status(404).send('Config antiga não encontrada');

  if (userConfig.configs.find(c => c.configname === newName))
    return res.status(400).send('Nome da nova config já existe');

  conf.configname = newName;
  writeJson(CONFIG_DB, configDB);

  res.send('Config renomeada com sucesso');
});

// UPDATE CONFIG
app.put('/updateconfig', requireJwt, (req, res) => {
  const username = req.auth.username;
  const { configname, config } = req.body;

  if (!isValidString(configname, 48) || !config)
    return res.status(400).send('Dados incompletos ou inválidos');

  const configDB = readJson(CONFIG_DB) || [];

  const userConfig = configDB.find(u => u.username === username);
  if (!userConfig) return res.status(404).send('Usuário não encontrado');

  const conf = userConfig.configs.find(c => c.configname === configname);
  if (!conf) return res.status(404).send('Config não encontrada');

  conf.config = config;
  writeJson(CONFIG_DB, configDB);

  res.send('Config atualizada com sucesso');
});

// PC
app.get('/pc/configs/:username', requireApiSecret, (req, res) => {
  const { username } = req.params;
  if (!isValidString(username)) return res.status(400).send('username inválido');

  const configDB = readJson(CONFIG_DB) || [];
  const userConfig = configDB.find(u => u.username === username);
  if (!userConfig) return res.json({ username, total: 0, configs: [] });

  const configs = userConfig.configs.map(c => ({
    configname: c.configname,
    config: c.config,
    visible: !!c.visible
  }));

  res.json({ username, total: configs.length, configs });
});

// PC
app.post('/pc/configs/:username', requireApiSecret, (req, res) => {
  const { username } = req.params;
  const { configname, config, visible } = req.body;

  if (!isValidString(username) || !isValidString(configname, 48) || !config)
    return res.status(400).send('Dados incompletos ou inválidos');

  if (!userExists(username)) return res.status(404).send('Usuário não encontrado');

  const configDB = readJson(CONFIG_DB) || [];
  const userConfig = getOrCreateUserConfig(configDB, username);

  if (userConfig.configs.find(c => c.configname === configname))
    return res.status(400).send('Config já existe');

  userConfig.configs.push({
    configname,
    config,
    description: null, // app do PC não define description
    visible: typeof visible === 'boolean' ? visible : false,
    likes: 0,
    likedBy: [],
    saves: 0
  });
  writeJson(CONFIG_DB, configDB);

  res.send('Config criada com sucesso');
});

// PC
app.put('/pc/configs/:username/:configname', requireApiSecret, (req, res) => {
  const { username, configname } = req.params;
  const { config } = req.body;

  if (!isValidString(username) || !isValidString(configname, 48) || !config)
    return res.status(400).send('Dados incompletos ou inválidos');

  const configDB = readJson(CONFIG_DB) || [];
  const userConfig = configDB.find(u => u.username === username);
  if (!userConfig) return res.status(404).send('Usuário não encontrado');

  const conf = userConfig.configs.find(c => c.configname === configname);
  if (!conf) return res.status(404).send('Config não encontrada');

  conf.config = config;
  writeJson(CONFIG_DB, configDB);

  res.send('Config atualizada com sucesso');
});

// PC
app.put('/pc/configs/:username/:configname/rename', requireApiSecret, (req, res) => {
  const { username, configname } = req.params;
  const { newName } = req.body;

  if (!isValidString(username) || !isValidString(configname, 48) || !isValidString(newName, 48))
    return res.status(400).send('Dados incompletos ou inválidos');

  const configDB = readJson(CONFIG_DB) || [];
  const userConfig = configDB.find(u => u.username === username);
  if (!userConfig) return res.status(404).send('Usuário não encontrado');

  const conf = userConfig.configs.find(c => c.configname === configname);
  if (!conf) return res.status(404).send('Config antiga não encontrada');

  if (userConfig.configs.find(c => c.configname === newName))
    return res.status(400).send('Nome da nova config já existe');

  conf.configname = newName;
  writeJson(CONFIG_DB, configDB);

  res.send('Config renomeada com sucesso');
});

// PC
app.delete('/pc/configs/:username/:configname', requireApiSecret, (req, res) => {
  const { username, configname } = req.params;

  if (!isValidString(username) || !isValidString(configname, 48))
    return res.status(400).send('Dados incompletos ou inválidos');

  const configDB = readJson(CONFIG_DB) || [];
  const userConfig = configDB.find(u => u.username === username);
  if (!userConfig) return res.status(404).send('Usuário não encontrado');

  const index = userConfig.configs.findIndex(c => c.configname === configname);
  if (index === -1) return res.status(404).send('Config não encontrada');

  userConfig.configs.splice(index, 1);
  writeJson(CONFIG_DB, configDB);

  res.send('Config deletada com sucesso');
});

// ADMIN
app.get('/admin/configs/:username', requireAdmin, (req, res) => {
  const { username } = req.params;
  if (!isValidString(username)) return res.status(400).send('username inválido');

  const configDB = readJson(CONFIG_DB) || [];
  const userConfig = configDB.find(u => u.username === username);
  if (!userConfig) return res.json({ username, total: 0, configs: [] });

  res.json({ username, total: userConfig.configs.length, configs: userConfig.configs });
});

// ADMIN
app.delete('/admin/configs/:username/:configname', requireAdmin, (req, res) => {
  const { username, configname } = req.params;

  if (!isValidString(username) || !isValidString(configname, 48))
    return res.status(400).send('Dados inválidos');

  const configDB = readJson(CONFIG_DB) || [];

  const userConfig = configDB.find(u => u.username === username);
  if (!userConfig) return res.status(404).send('Usuário não encontrado');

  const index = userConfig.configs.findIndex(c => c.configname === configname);
  if (index === -1) return res.status(404).send('Config não encontrada');

  const [removed] = userConfig.configs.splice(index, 1);
  writeJson(CONFIG_DB, configDB);

  console.log(`[ADMIN] ${req.auth.username} deletou config "${configname}" de "${username}" (visible=${removed.visible})`);
  res.json({ message: 'Config removida pela moderação', configname, owner: username, wasPublic: !!removed.visible });
});

// START SERVER 
try {
  const server = https.createServer(httpsOptions, app);
  //const server = http.createServer(app);

  server.listen(PORT, () => {
    console.log(`API HTTPS rodando em https://weedsolution.xyz:${PORT}`);
  });

  server.on('error', (err) => {
    console.error('Erro ao iniciar o servidor HTTPS:', err.message);
  });

} catch (err) {
  console.error('Falha ao criar o servidor HTTPS:', err);
}
