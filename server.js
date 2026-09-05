// Leaderboard backend. Node standard library only, no dependencies.
//
// Run:
//     node server.js
//
// Then open http://localhost:3000 in a browser.

const http = require("node:http");
const fs = require("node:fs");
const path = require("node:path");

const PORT = 3000;
const HOST = "127.0.0.1"; // loopback only, so nothing else on the LAN can post scores
const MAX_ROUNDS = 50; // keep in step with MAX_ROUNDS in memory_game.ino
const MAX_BODY_BYTES = 4096;

const SCORES_FILE = path.join(__dirname, "scores.json");
const TMP_FILE = SCORES_FILE + ".tmp";
const INDEX_FILE = path.join(__dirname, "public", "index.html");

function loadScores() {
  try {
    const parsed = JSON.parse(fs.readFileSync(SCORES_FILE, "utf8"));
    return Array.isArray(parsed) ? parsed : [];
  } catch {
    // missing or corrupt file, just start empty
    return [];
  }
}

// temp file + rename, so a crash mid-write can't corrupt scores.json
function saveScores(scores) {
  fs.writeFileSync(TMP_FILE, JSON.stringify(scores, null, 2));
  fs.renameSync(TMP_FILE, SCORES_FILE);
}

function sendJson(res, status, body) {
  const payload = JSON.stringify(body);
  res.writeHead(status, {
    "Content-Type": "application/json",
    "Content-Length": Buffer.byteLength(payload),
  });
  res.end(payload);
}

function readBody(req, limit) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    let size = 0;

    req.on("data", (chunk) => {
      size += chunk.length;
      if (size > limit) {
        // cap it. listener.py only ever sends a few dozen bytes
        req.destroy();
        reject(new Error("body too large"));
        return;
      }
      chunks.push(chunk);
    });
    req.on("end", () => resolve(Buffer.concat(chunks).toString("utf8")));
    req.on("error", reject);
  });
}

async function handle(req, res) {
  const { pathname } = new URL(req.url, `http://${req.headers.host || HOST}`);

  if (req.method === "GET" && pathname === "/scores") {
    return sendJson(res, 200, loadScores().sort((a, b) => b.score - a.score));
  }

  if (req.method === "POST" && pathname === "/scores") {
    let body;
    try {
      body = JSON.parse(await readBody(req, MAX_BODY_BYTES));
    } catch {
      return sendJson(res, 400, { error: "invalid JSON body" });
    }

    const { score, player } = body || {};

    if (!Number.isInteger(score) || score < 0 || score > MAX_ROUNDS) {
      return sendJson(res, 400, {
        error: `score must be a whole number between 0 and ${MAX_ROUNDS}`,
      });
    }
    if (player != null && ![1, 2, 3].includes(player)) {
      return sendJson(res, 400, { error: "player must be 1, 2, 3, or null" });
    }

    const scores = loadScores();
    scores.push({
      player: player ?? null,
      score,
      timestamp: new Date().toISOString(),
    });
    saveScores(scores);

    return sendJson(res, 201, { message: "Score saved" });
  }

  // only one static file, so no path mapping needed
  if (req.method === "GET" && (pathname === "/" || pathname === "/index.html")) {
    const html = fs.readFileSync(INDEX_FILE);
    res.writeHead(200, {
      "Content-Type": "text/html; charset=utf-8",
      "Content-Length": html.length,
    });
    return res.end(html);
  }

  sendJson(res, 404, { error: "not found" });
}

http
  .createServer((req, res) => {
    handle(req, res).catch((err) => {
      console.error("Request failed:", err);
      if (!res.headersSent) sendJson(res, 500, { error: "internal error" });
    });
  })
  .listen(PORT, HOST, () => {
    console.log(`Leaderboard server running at http://localhost:${PORT}`);
  });
