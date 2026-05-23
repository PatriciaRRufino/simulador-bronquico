/*
  ================================================================
    SIMULADOR DE TREINAMENTO BRÔNQUICO
    Código ESP32 - sensor_bronquico.ino
    Versão: 3.0 — PWA + múltiplos pontos + sensor de proximidade
  ================================================================
  COMO USAR:
    1. Grave este código no ESP32
    2. Ligue o ESP32 na tomada
    3. LED pisca 3x → sistema pronto
    4. No celular: conecte no Wi-Fi "SimuladorBronquico" (senha: bronquio123)
    5. Aviso "sem internet" → toque em "Manter conexão"
    6. Abra o navegador → acesse 192.168.4.1
    7. Toque em "Adicionar à tela inicial" para instalar o app

  HARDWARE:
    Pino 4  → Ponto 1
    Pino 15 → Ponto 2
    Pino 13 → Ponto 3
    Pino 12 → Ponto 4
    Pino 14 → Ponto 5
    Pino 27 → Ponto 6

  PARA ADICIONAR/REMOVER PONTOS:
    Veja a seção "CONFIGURAÇÃO DOS PONTOS" abaixo
  ================================================================
*/

#include <WiFi.h>
#include <WebServer.h>

/* ── REDE WI-FI DO ESP32 ─────────────────────────────────────── */
const char* AP_SSID     = "SimuladorBronquico";
const char* AP_PASSWORD = "bronquio123";

/* ── LED ─────────────────────────────────────────────────────── */
const int PINO_LED = 2;

/* ================================================================
   CONFIGURAÇÃO DOS PONTOS
   
   Para adicionar um ponto: copie uma linha e mude o número
   Para remover um ponto:   apague a linha
   Para renomear:           mude o texto entre aspas no nome
   Para trocar o pino:      mude o número em gpio
   
   PINOS TOUCH DISPONÍVEIS: 4, 0, 2, 13, 12, 14, 27, 15, 33, 32
   (evite o 2 se estiver usando como LED)
   ================================================================ */
struct PontoSensor {
  const char* nome;   // Nome exibido na interface
  int         gpio;   // Pino físico do ESP32
};

// ⚠️  EDITE AQUI para adicionar, remover ou renomear pontos:
PontoSensor pontos[] = {
  { "Ponto 1",  4  },
  { "Ponto 2",  15 },
  { "Ponto 3",  13 },
  { "Ponto 4",  12 },
  { "Ponto 5",  14 },
  { "Ponto 6",  27 },
};
const int TOTAL_PONTOS = sizeof(pontos) / sizeof(pontos[0]);

/* ── SENSIBILIDADE ───────────────────────────────────────────── */
/*
  Sensor de PROXIMIDADE (broncoscópio passa perto):
    Aumente o limiar para detectar a distância certa.
    Sem proximidade: ~900   |   Com proximidade: ~400-600
    Sugestão inicial: 700 — ajuste conforme seu teste

  Para CALIBRAR: descomente a linha de debug no loop()
  e observe os valores no Monitor Serial.
*/
int limiarToque = 1000;  // Pode ser ajustado sem recompilar pela interface

/* ── DEBOUNCE ────────────────────────────────────────────────── */
const unsigned long DEBOUNCE_MS = 800;
unsigned long ultimoDisparo[10] = {0}; // Suporta até 10 pontos

/* ── SERVIDOR WEB ────────────────────────────────────────────── */
WebServer server(80);

/* ── ESTADO DA SESSÃO ────────────────────────────────────────── */
bool   testeAtivo      = false;
String nomeAluno       = "";
String inicioTeste     = "";
int    totalDeteccoes  = 0;

struct Deteccao {
  String ponto;
  String timestamp;
};
Deteccao deteccoes[300]; // Suporta até 300 detecções por sessão

/* ── DECLARAÇÕES ANTECIPADAS ─────────────────────────────────── */
void handleIndex();
void handleIniciar();
void handleEncerrar();
void handleEstado();
void handleRelatorio();
void handleConfig();
void registrarDeteccao(int indicePonto);
void piscarLED();
String gerarCardsRegioes();
String gerarLogJS();


/* ================================================================
   SETUP
   ================================================================ */
void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(PINO_LED, OUTPUT);
  digitalWrite(PINO_LED, LOW);

  Serial.println("\n--- Simulador Bronquico v3.0 ---");
  Serial.printf("Pontos configurados: %d\n", TOTAL_PONTOS);
  for (int i = 0; i < TOTAL_PONTOS; i++) {
    Serial.printf("  %s → GPIO %d\n", pontos[i].nome, pontos[i].gpio);
  }

  // Inicia Access Point
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("Rede Wi-Fi criada: %s\n", AP_SSID);
  Serial.println("Acesse: http://192.168.4.1");

  // Rotas do servidor
  server.on("/",          HTTP_GET,  handleIndex);
  server.on("/iniciar",   HTTP_POST, handleIniciar);
  server.on("/encerrar",  HTTP_POST, handleEncerrar);
  server.on("/estado",    HTTP_GET,  handleEstado);
  server.on("/relatorio", HTTP_GET,  handleRelatorio);
  server.on("/config",    HTTP_POST, handleConfig);
  server.begin();

  // Pisca LED 3x — sistema pronto
  for (int i = 0; i < 3; i++) {
    digitalWrite(PINO_LED, HIGH); delay(200);
    digitalWrite(PINO_LED, LOW);  delay(200);
  }
  Serial.println("Sistema pronto!");
}


/* ================================================================
   LOOP PRINCIPAL
   ================================================================ */
void loop() {
  server.handleClient();

  unsigned long agora = millis();

  for (int i = 0; i < TOTAL_PONTOS; i++) {
    int leitura = touchRead(pontos[i].gpio);

    // Descomente para calibrar no Monitor Serial:
    // Serial.printf("%s: %d\n", pontos[i].nome, leitura);

    if (leitura < limiarToque && (agora - ultimoDisparo[i]) >= DEBOUNCE_MS) {
      registrarDeteccao(i);
      ultimoDisparo[i] = agora;
    }
  }

  delay(30);
}


/* ================================================================
   FUNÇÃO: registrarDeteccao
   ================================================================ */
void registrarDeteccao(int idx) {
  Serial.printf("Deteccao: %s (GPIO %d)\n", pontos[idx].nome, pontos[idx].gpio);
  piscarLED();

  if (!testeAtivo) {
    Serial.println("  (ignorada — sem teste ativo)");
    return;
  }
  if (totalDeteccoes >= 300) {
    Serial.println("  (ignorada — limite atingido)");
    return;
  }

  deteccoes[totalDeteccoes].ponto     = String(pontos[idx].nome);
  deteccoes[totalDeteccoes].timestamp = String(millis() / 1000) + "s";
  totalDeteccoes++;
  Serial.printf("  Registrada! Total: %d\n", totalDeteccoes);
}


/* ================================================================
   ROTA: / — Interface HTML com PWA
   ================================================================ */
void handleIndex() {
  server.sendHeader("Access-Control-Allow-Origin", "*");

  // Gera os cards de pontos dinamicamente
  String cards = "";
  for (int i = 0; i < TOTAL_PONTOS; i++) {
    String id = String(pontos[i].nome);
    id.replace(" ", "_");
    id.toLowerCase();
    cards += "<div class='regiao-card' id='card-" + id + "'>";
    cards += "<span class='r-icon'>⚪</span>";
    cards += "<span class='r-nome'>" + String(pontos[i].nome) + "</span>";
    cards += "<span class='r-count' id='count-" + id + "'>0</span>";
    cards += "</div>";
  }

  // Gera array de pontos para o JavaScript
  String pontosJS = "[";
  for (int i = 0; i < TOTAL_PONTOS; i++) {
    String id = String(pontos[i].nome);
    id.replace(" ", "_");
    id.toLowerCase();
    if (i > 0) pontosJS += ",";
    pontosJS += "{\"nome\":\"" + String(pontos[i].nome) + "\",\"id\":\"" + id + "\"}";
  }
  pontosJS += "]";

  String html = R"rawhtml(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0"/>
<title>Simulador Brônquico</title>
<meta name="mobile-web-app-capable" content="yes"/>
<meta name="apple-mobile-web-app-capable" content="yes"/>
<meta name="apple-mobile-web-app-title" content="Sim. Bronquico"/>
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent"/>
<meta name="theme-color" content="#0b0f1a"/>
<link rel="apple-touch-icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><rect width='100' height='100' rx='20' fill='%230b0f1a'/><text y='72' x='50' text-anchor='middle' font-size='60'>🫁</text></svg>"/>
<link rel="icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><rect width='100' height='100' rx='20' fill='%230b0f1a'/><text y='72' x='50' text-anchor='middle' font-size='60'>🫁</text></svg>"/>
<style>
:root{--bg:#0b0f1a;--surface:#111827;--border:#1e2d45;--text:#e2e8f0;--muted:#64748b;--blue:#38bdf8;--blue-dim:#0c4a6e;--green:#4ade80;--green-dim:#14532d;--red:#f87171;--radius:14px;}
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0;}
html{font-size:16px;-webkit-tap-highlight-color:transparent;}
body{background:var(--bg);color:var(--text);font-family:system-ui,sans-serif;min-height:100dvh;padding:0 0 80px;}
header{background:var(--surface);border-bottom:1px solid var(--border);padding:18px 20px 14px;display:flex;align-items:center;gap:12px;position:sticky;top:0;z-index:100;}
.logo-icon{width:40px;height:40px;background:linear-gradient(135deg,#0ea5e9,#6366f1);border-radius:10px;display:grid;place-items:center;font-size:20px;flex-shrink:0;}
.logo-text strong{display:block;font-size:15px;font-weight:800;}
.logo-text span{font-size:11px;color:var(--muted);font-family:monospace;}
#status-badge{margin-left:auto;padding:5px 12px;border-radius:99px;font-size:11px;font-family:monospace;border:1px solid var(--border);color:var(--muted);background:transparent;transition:all .3s;white-space:nowrap;}
#status-badge.ativo{color:var(--green);border-color:var(--green);background:var(--green-dim);animation:pulse 2s infinite;}
@keyframes pulse{0%,100%{opacity:1;}50%{opacity:.6;}}
.section{padding:20px 16px 0;}
.card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius);padding:18px;margin-bottom:12px;}
.card-title{font-size:11px;font-family:monospace;color:var(--muted);text-transform:uppercase;letter-spacing:1px;margin-bottom:14px;}
label{display:block;font-size:13px;color:var(--muted);margin-bottom:6px;font-family:monospace;}
input[type=text]{width:100%;padding:13px 14px;background:var(--bg);border:1px solid var(--border);border-radius:10px;color:var(--text);font-size:16px;font-weight:600;outline:none;-webkit-appearance:none;}
input[type=text]:focus{border-color:var(--blue);}
input[type=text]::placeholder{color:var(--muted);font-weight:400;}
.btn{display:flex;align-items:center;justify-content:center;gap:8px;width:100%;padding:15px;border:none;border-radius:12px;font-size:15px;font-weight:700;cursor:pointer;transition:opacity .15s,transform .1s;-webkit-appearance:none;}
.btn:active{transform:scale(.97);opacity:.85;}
.btn:disabled{opacity:.3;cursor:not-allowed;transform:none;}
.btn+.btn{margin-top:8px;}
.btn-iniciar{background:var(--blue);color:#000;}
.btn-encerrar{background:var(--red);color:#fff;}
.btn-csv{background:var(--green);color:#000;}
.btn-outline{background:transparent;border:1px solid var(--border);color:var(--muted);}
.regioes-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;}
.regiao-card{background:var(--bg);border:1px solid var(--border);border-radius:12px;padding:12px 8px;text-align:center;transition:border-color .3s,background .3s;}
.regiao-card.flash{background:rgba(56,189,248,.15);border-color:var(--blue);}
.r-icon{font-size:20px;display:block;margin-bottom:4px;}
.r-nome{font-size:10px;font-family:monospace;color:var(--muted);display:block;margin-bottom:6px;line-height:1.3;}
.r-count{font-size:26px;font-weight:800;display:block;color:var(--blue);}
#log-lista{list-style:none;display:flex;flex-direction:column;gap:6px;max-height:240px;overflow-y:auto;}
#log-lista:empty::after{content:"Nenhuma detecção ainda…";display:block;text-align:center;color:var(--muted);font-family:monospace;font-size:13px;padding:20px 0;}
.log-item{display:flex;align-items:center;gap:10px;padding:10px 12px;background:var(--bg);border-radius:8px;border-left:3px solid var(--blue);animation:slide-in .25s ease;}
@keyframes slide-in{from{opacity:0;transform:translateX(-10px);}to{opacity:1;transform:translateX(0);}}
.log-item .l-num{font-family:monospace;font-size:11px;color:var(--muted);min-width:22px;}
.log-item .l-nome{flex:1;font-size:13px;font-weight:600;}
.log-item .l-hora{font-family:monospace;font-size:11px;color:var(--muted);}
#aluno-ativo-banner{display:none;align-items:center;gap:10px;background:var(--blue-dim);border:1px solid var(--blue);border-radius:10px;padding:12px 14px;margin-bottom:12px;}
#aluno-ativo-banner.visible{display:flex;}
.ab-label{font-size:11px;color:var(--blue);font-family:monospace;display:block;}
.ab-nome{font-size:15px;font-weight:700;}
#timer{font-family:monospace;font-size:13px;color:var(--muted);text-align:center;padding:4px 0 10px;display:none;}
#toast{position:fixed;bottom:24px;left:50%;transform:translateX(-50%) translateY(80px);background:var(--surface);border:1px solid var(--border);color:var(--text);padding:12px 20px;border-radius:99px;font-size:13px;font-weight:600;white-space:nowrap;transition:transform .3s,opacity .3s;opacity:0;pointer-events:none;z-index:999;}
#toast.show{transform:translateX(-50%) translateY(0);opacity:1;}
#toast.ok{border-color:var(--green);color:var(--green);}
#toast.err{border-color:var(--red);color:var(--red);}
</style>
</head>
<body>
<header>
  <div class="logo-icon">🫁</div>
  <div class="logo-text">
    <strong>Simulador Brônquico</strong>
    <span>Centro Cirúrgico · v3.0</span>
  </div>
  <div id="status-badge">INATIVO</div>
</header>
<div class="section">
  <div class="card" id="card-inicio">
    <div class="card-title">▸ Nova Sessão</div>
    <label for="nome-aluno">Nome do Aluno</label>
    <input type="text" id="nome-aluno" placeholder="Ex: João da Silva" autocomplete="off" autocapitalize="words"/>
    <div style="height:12px"></div>
    <button class="btn btn-iniciar" id="btn-iniciar" onclick="iniciarTeste()">▶ Iniciar Teste</button>
  </div>
  <div id="aluno-ativo-banner">
    <span style="font-size:18px">👤</span>
    <div>
      <span class="ab-label">TESTE EM ANDAMENTO</span>
      <div class="ab-nome" id="banner-nome-aluno">—</div>
    </div>
  </div>
  <div id="timer">⏱ Tempo: <span id="timer-valor">00:00</span></div>
  <div class="card">
    <div class="card-title">▸ Detecções por Ponto</div>
    <div class="regioes-grid" id="grid-pontos">)rawhtml";

  html += cards;

  html += R"rawhtml(
    </div>
  </div>
  <div class="card">
    <div class="card-title">▸ Log de Detecções</div>
    <ul id="log-lista"></ul>
  </div>
  <div class="card">
    <div class="card-title">▸ Ações</div>
    <button class="btn btn-encerrar" id="btn-encerrar" onclick="encerrarTeste()" disabled>⏹ Encerrar Teste</button>
    <button class="btn btn-csv" id="btn-csv" onclick="baixarCSV()" disabled>⬇ Salvar Relatório CSV</button>
    <button class="btn btn-outline" onclick="limparTela()">🗑 Limpar Tela</button>
  </div>
</div>
<div id="toast"></div>
<script>
const PONTOS = )rawhtml";

  html += pontosJS;

  html += R"rawhtml(;
let testeAtivo=false,totalAntes=0,pollingTimer=null,timerInterval=null,inicioTeste=null;

function iniciarPolling(){if(pollingTimer)clearInterval(pollingTimer);pollingTimer=setInterval(consultarEstado,2000);}
function pararPolling(){if(pollingTimer)clearInterval(pollingTimer);pollingTimer=null;}

async function consultarEstado(){
  try{
    const r=await fetch("/estado");
    if(!r.ok)return;
    const d=await r.json();
    atualizarInterface(d);
  }catch(e){}
}

function atualizarInterface(data){
  PONTOS.forEach(p=>{
    const c=data.deteccoes.filter(d=>d.ponto===p.nome).length;
    const el=document.getElementById("count-"+p.id);
    if(el)el.textContent=c;
  });
  if(data.total>totalAntes){
    const novas=data.deteccoes.slice(totalAntes);
    novas.forEach((det,i)=>adicionarLogItem(det,totalAntes+i+1));
    const ultima=novas[novas.length-1];
    const p=PONTOS.find(p=>p.nome===ultima.ponto);
    if(p){
      const card=document.getElementById("card-"+p.id);
      if(card){card.classList.add("flash");setTimeout(()=>card.classList.remove("flash"),600);}
    }
    totalAntes=data.total;
  }
}

function adicionarLogItem(det,num){
  const lista=document.getElementById("log-lista");
  const li=document.createElement("li");
  li.className="log-item";
  li.innerHTML=`<span class="l-num">#${num}</span><span class="l-nome">${det.ponto}</span><span class="l-hora">${det.timestamp}</span>`;
  lista.prepend(li);
}

async function iniciarTeste(){
  const nome=document.getElementById("nome-aluno").value.trim();
  if(!nome){mostrarToast("Informe o nome do aluno.","err");return;}
  document.getElementById("btn-iniciar").disabled=true;
  try{
    const r=await fetch("/iniciar",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({aluno:nome})});
    const d=await r.json();
    if(!d.ok){mostrarToast(d.erro||"Erro ao iniciar.","err");document.getElementById("btn-iniciar").disabled=false;return;}
    testeAtivo=true;totalAntes=0;
    document.getElementById("log-lista").innerHTML="";
    PONTOS.forEach(p=>{const el=document.getElementById("count-"+p.id);if(el)el.textContent="0";});
    document.getElementById("aluno-ativo-banner").classList.add("visible");
    document.getElementById("banner-nome-aluno").textContent=nome;
    document.getElementById("status-badge").textContent="● ATIVO";
    document.getElementById("status-badge").classList.add("ativo");
    document.getElementById("card-inicio").style.opacity=".4";
    document.getElementById("card-inicio").style.pointerEvents="none";
    document.getElementById("btn-encerrar").disabled=false;
    document.getElementById("btn-csv").disabled=true;
    document.getElementById("timer").style.display="block";
    inicioTeste=Date.now();
    timerInterval=setInterval(()=>{
      const s=Math.floor((Date.now()-inicioTeste)/1000);
      document.getElementById("timer-valor").textContent=String(Math.floor(s/60)).padStart(2,"0")+":"+String(s%60).padStart(2,"0");
    },1000);
    iniciarPolling();
    mostrarToast("✅ Teste iniciado para "+nome,"ok");
  }catch(e){mostrarToast("Sem conexão com o ESP32!","err");document.getElementById("btn-iniciar").disabled=false;}
}

async function encerrarTeste(){
  if(!confirm("Encerrar o teste agora?"))return;
  try{
    const r=await fetch("/encerrar",{method:"POST"});
    const d=await r.json();
    if(!d.ok){mostrarToast(d.erro||"Erro.","err");return;}
    await consultarEstado();
    pararPolling();clearInterval(timerInterval);
    testeAtivo=false;
    document.getElementById("status-badge").textContent="ENCERRADO";
    document.getElementById("status-badge").classList.remove("ativo");
    document.getElementById("btn-encerrar").disabled=true;
    document.getElementById("btn-csv").disabled=false;
    mostrarToast("🛑 Encerrado — "+d.total_deteccoes+" detecção(ões)","ok");
  }catch(e){mostrarToast("Sem conexão!","err");}
}

function baixarCSV(){window.location.href="/relatorio";mostrarToast("⬇ Baixando CSV…","ok");}

function limparTela(){
  if(testeAtivo){mostrarToast("Encerre o teste antes.","err");return;}
  document.getElementById("log-lista").innerHTML="";
  PONTOS.forEach(p=>{const el=document.getElementById("count-"+p.id);if(el)el.textContent="0";});
  document.getElementById("nome-aluno").value="";
  document.getElementById("aluno-ativo-banner").classList.remove("visible");
  document.getElementById("card-inicio").style.opacity="1";
  document.getElementById("card-inicio").style.pointerEvents="auto";
  document.getElementById("btn-iniciar").disabled=false;
  document.getElementById("btn-csv").disabled=true;
  document.getElementById("timer").style.display="none";
  document.getElementById("timer-valor").textContent="00:00";
  document.getElementById("status-badge").textContent="INATIVO";
  totalAntes=0;
}

let toastTimer=null;
function mostrarToast(msg,tipo){
  const t=document.getElementById("toast");
  t.textContent=msg;t.className="show "+tipo;
  if(toastTimer)clearTimeout(toastTimer);
  toastTimer=setTimeout(()=>{t.className="";},3200);
}

// Verifica sessão ativa ao carregar
window.addEventListener("DOMContentLoaded",async()=>{
  try{
    const r=await fetch("/estado");
    const d=await r.json();
    if(d.ativa){
      testeAtivo=true;totalAntes=0;
      d.deteccoes.forEach((det,i)=>adicionarLogItem(det,i+1));
      atualizarInterface(d);totalAntes=d.total;
      document.getElementById("aluno-ativo-banner").classList.add("visible");
      document.getElementById("banner-nome-aluno").textContent=d.aluno;
      document.getElementById("status-badge").textContent="● ATIVO";
      document.getElementById("status-badge").classList.add("ativo");
      document.getElementById("card-inicio").style.opacity=".4";
      document.getElementById("card-inicio").style.pointerEvents="none";
      document.getElementById("btn-encerrar").disabled=false;
      document.getElementById("timer").style.display="block";
      inicioTeste=Date.now();
      timerInterval=setInterval(()=>{
        const s=Math.floor((Date.now()-inicioTeste)/1000);
        document.getElementById("timer-valor").textContent=String(Math.floor(s/60)).padStart(2,"0")+":"+String(s%60).padStart(2,"0");
      },1000);
      iniciarPolling();
    }
  }catch(e){}
});
</script>
</body>
</html>)rawhtml";

  server.send(200, "text/html; charset=utf-8", html);
}


/* ================================================================
   ROTA: /iniciar
   ================================================================ */
void handleIniciar() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"erro\":\"Body ausente.\"}");
    return;
  }
  String body = server.arg("plain");
  int idx = body.indexOf("\"aluno\"");
  if (idx == -1) {
    server.send(400, "application/json", "{\"ok\":false,\"erro\":\"Campo aluno ausente.\"}");
    return;
  }
  int ini = body.indexOf("\"", idx + 7) + 1;
  int fim = body.indexOf("\"", ini);
  nomeAluno = body.substring(ini, fim);
  nomeAluno.trim();
  if (nomeAluno.length() == 0) {
    server.send(400, "application/json", "{\"ok\":false,\"erro\":\"Nome vazio.\"}");
    return;
  }
  testeAtivo     = true;
  totalDeteccoes = 0;
  inicioTeste    = String(millis() / 1000) + "s";
  Serial.println("Teste INICIADO: " + nomeAluno);
  server.send(200, "application/json", "{\"ok\":true,\"mensagem\":\"Iniciado\"}");
}


/* ================================================================
   ROTA: /encerrar
   ================================================================ */
void handleEncerrar() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (!testeAtivo) {
    server.send(400, "application/json", "{\"ok\":false,\"erro\":\"Nenhum teste ativo.\"}");
    return;
  }
  testeAtivo = false;
  Serial.println("Teste ENCERRADO: " + nomeAluno);
  server.send(200, "application/json",
    "{\"ok\":true,\"mensagem\":\"Encerrado\",\"total_deteccoes\":" + String(totalDeteccoes) + "}");
}


/* ================================================================
   ROTA: /estado
   ================================================================ */
void handleEstado() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String json = "{";
  json += "\"ativa\":"   + String(testeAtivo ? "true" : "false") + ",";
  json += "\"aluno\":\""  + nomeAluno + "\",";
  json += "\"inicio\":\""  + inicioTeste + "\",";
  json += "\"total\":"   + String(totalDeteccoes) + ",";
  json += "\"deteccoes\":[";
  for (int i = 0; i < totalDeteccoes; i++) {
    if (i > 0) json += ",";
    json += "{\"ponto\":\"" + deteccoes[i].ponto + "\",\"timestamp\":\"" + deteccoes[i].timestamp + "\"}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}


/* ================================================================
   ROTA: /relatorio — CSV para download
   ================================================================ */
void handleRelatorio() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Content-Disposition", "attachment; filename=relatorio_" + nomeAluno + ".csv");

  String csv = "SIMULADOR DE TREINAMENTO BRONQUICO\n";
  csv += "Aluno," + nomeAluno + "\n";
  csv += "Inicio," + inicioTeste + "\n";
  csv += "Total de Deteccoes," + String(totalDeteccoes) + "\n\n";

  // Resumo por ponto
  csv += "RESUMO POR PONTO\n";
  csv += "Ponto,Deteccoes\n";
  for (int i = 0; i < TOTAL_PONTOS; i++) {
    int count = 0;
    for (int j = 0; j < totalDeteccoes; j++) {
      if (deteccoes[j].ponto == String(pontos[i].nome)) count++;
    }
    csv += String(pontos[i].nome) + "," + String(count) + "\n";
  }

  // Log detalhado
  csv += "\nLOG DETALHADO\n";
  csv += "#,Ponto,Timestamp\n";
  for (int i = 0; i < totalDeteccoes; i++) {
    csv += String(i + 1) + "," + deteccoes[i].ponto + "," + deteccoes[i].timestamp + "\n";
  }

  server.send(200, "text/csv", csv);
}


/* ================================================================
   ROTA: /config — Ajusta o limiar sem recompilar
   Body JSON: { "limiar": 700 }
   ================================================================ */
void handleConfig() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  String body = server.arg("plain");
  int idx = body.indexOf("\"limiar\"");
  if (idx != -1) {
    int ini = idx + 8;
    while (body[ini] == ' ' || body[ini] == ':') ini++;
    limiarToque = body.substring(ini).toInt();
    Serial.printf("Limiar atualizado: %d\n", limiarToque);
    server.send(200, "application/json", "{\"ok\":true,\"limiar\":" + String(limiarToque) + "}");
  } else {
    server.send(400, "application/json", "{\"ok\":false,\"erro\":\"Campo limiar ausente.\"}");
  }
}


/* ================================================================
   FUNÇÃO: piscarLED
   ================================================================ */
void piscarLED() {
  digitalWrite(PINO_LED, HIGH);
  delay(200);
  digitalWrite(PINO_LED, LOW);
}
