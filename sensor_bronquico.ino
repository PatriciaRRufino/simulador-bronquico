/*
  ================================================================
    SIMULADOR DE TREINAMENTO BRÔNQUICO
    Código ESP32 - sensor_bronquico.ino
    Versão: 2.0 — ESP32 autossuficiente
  ================================================================
  O ESP32 agora faz TUDO sozinho:
    ✅ Cria a própria rede Wi-Fi
    ✅ Hospeda o servidor web
    ✅ Lê os sensores capacitivos
    ✅ Serve a interface para o celular
    ✅ Gera o CSV para download

  COMO USAR:
    1. Grave este código no ESP32
    2. Ligue o ESP32 na tomada
    3. No celular, conecte no Wi-Fi "SimuladorBronquico"
       Senha: bronquio123
    4. Abra o navegador e acesse: 192.168.4.1
    5. Pronto!

  HARDWARE:
    Pino 4  → Traqueia
    Pino 15 → Brônquio Direito
    Pino 13 → Brônquio Esquerdo
    Pino 2  → LED interno
  ================================================================
*/

#include <WiFi.h>
#include <WebServer.h>

/* ── CONFIGURAÇÕES DO ACCESS POINT (rede própria do ESP32) ───── */
const char* AP_SSID     = "SimuladorBronquico";
const char* AP_PASSWORD = "bronquio123";

/* ── PINOS ───────────────────────────────────────────────────── */
const int pinoTraqueia  = 4;
const int pinoBronquioD = 15;
const int pinoBronquioE = 13;
const int PINO_LED      = 2;

/* ── SENSIBILIDADE ───────────────────────────────────────────── */
const int limiarToque = 500;

/* ── DEBOUNCE ────────────────────────────────────────────────── */
const unsigned long DEBOUNCE_MS = 1000;
unsigned long ultimoTraqueia = 0;
unsigned long ultimoBronqD   = 0;
unsigned long ultimoBronqE   = 0;

/* ── SERVIDOR WEB ────────────────────────────────────────────── */
WebServer server(80);

/* ── DECLARAÇÃO ANTECIPADA DAS FUNÇÕES ───────────────────────── */
void handleIndex();
void handleIniciar();
void handleEncerrar();
void handleEstado();
void handleRelatorio();
void registrarDeteccao(String regiao);
void piscarLED();

/* ── ESTADO DA SESSÃO ────────────────────────────────────────── */
bool   testeAtivo   = false;
String nomeAluno    = "";
String inicioTeste  = "";

// Armazena até 200 detecções na memória
struct Deteccao {
  String regiao;
  String timestamp;
};
Deteccao deteccoes[200];
int totalDeteccoes = 0;


/* ================================================================
   SETUP
   ================================================================ */
void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(PINO_LED, OUTPUT);
  digitalWrite(PINO_LED, LOW);

  Serial.println("\n--- Simulador Bronquico v2.0 ---");

  // Inicia o Access Point
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.println("Rede Wi-Fi criada!");
  Serial.print("  Nome: ");
  Serial.println(AP_SSID);
  Serial.print("  Senha: ");
  Serial.println(AP_PASSWORD);
  Serial.println("  Acesse: http://192.168.4.1");

  // Pisca LED 3x para indicar que está pronto
  for (int i = 0; i < 3; i++) {
    digitalWrite(PINO_LED, HIGH); delay(200);
    digitalWrite(PINO_LED, LOW);  delay(200);
  }

  // Configura as rotas do servidor
  server.on("/",          HTTP_GET,  handleIndex);
  server.on("/iniciar",   HTTP_POST, handleIniciar);
  server.on("/encerrar",  HTTP_POST, handleEncerrar);
  server.on("/estado",    HTTP_GET,  handleEstado);
  server.on("/relatorio", HTTP_GET,  handleRelatorio);
  server.begin();

  Serial.println("Servidor web iniciado!");
}


/* ================================================================
   LOOP PRINCIPAL
   ================================================================ */
void loop() {
  server.handleClient(); // Processa requisições web

  unsigned long agora = millis();

  int leituraTraqueia  = touchRead(pinoTraqueia);
  int leituraBronquioD = touchRead(pinoBronquioD);
  int leituraBronquioE = touchRead(pinoBronquioE);

  // Descomente para calibrar:
  // Serial.printf("T:%d | D:%d | E:%d\n", leituraTraqueia, leituraBronquioD, leituraBronquioE);

  if (leituraTraqueia < limiarToque && (agora - ultimoTraqueia) >= DEBOUNCE_MS) {
    registrarDeteccao("traqueia");
    ultimoTraqueia = agora;
  }

  if (leituraBronquioD < limiarToque && (agora - ultimoBronqD) >= DEBOUNCE_MS) {
    registrarDeteccao("bronquio_direito");
    ultimoBronqD = agora;
  }

  if (leituraBronquioE < limiarToque && (agora - ultimoBronqE) >= DEBOUNCE_MS) {
    registrarDeteccao("bronquio_esquerdo");
    ultimoBronqE = agora;
  }
}


/* ================================================================
   FUNÇÃO: registrarDeteccao
   ================================================================ */
void registrarDeteccao(String regiao) {
  Serial.println("Deteccao: " + regiao);
  piscarLED();

  if (!testeAtivo) {
    Serial.println("  (ignorada — sem teste ativo)");
    return;
  }

  if (totalDeteccoes >= 200) {
    Serial.println("  (ignorada — limite de 200 deteccoes atingido)");
    return;
  }

  deteccoes[totalDeteccoes].regiao    = regiao;
  deteccoes[totalDeteccoes].timestamp = String(millis() / 1000) + "s";
  totalDeteccoes++;

  Serial.printf("  Registrada! Total: %d\n", totalDeteccoes);
}


/* ================================================================
   ROTA: / — Página principal (interface HTML)
   ================================================================ */
void handleIndex() {
  // CORS para aceitar requisições do celular
  server.sendHeader("Access-Control-Allow-Origin", "*");

  String html = R"rawhtml(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0"/>
  <title>Simulador Brônquico</title>
  <link rel="manifest" href="data:application/json,{&quot;name&quot;:&quot;Simulador Bronquico&quot;,&quot;short_name&quot;:&quot;Bronquico&quot;,&quot;start_url&quot;:&quot;/&quot;,&quot;display&quot;:&quot;standalone&quot;,&quot;background_color&quot;:&quot;#0b0f1a&quot;,&quot;theme_color&quot;:&quot;#38bdf8&quot;,&quot;icons&quot;:[{&quot;src&quot;:&quot;data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>🫁</text></svg>&quot;,&quot;sizes&quot;:&quot;any&quot;,&quot;type&quot;:&quot;image/svg+xml&quot;}]}"/>
  <meta name="mobile-web-app-capable" content="yes"/>
  <meta name="apple-mobile-web-app-capable" content="yes"/>
  <meta name="apple-mobile-web-app-title" content="Simulador Bronquico"/>
  <meta name="theme-color" content="#0b0f1a"/>
  <style>
    :root{--bg:#0b0f1a;--surface:#111827;--border:#1e2d45;--text:#e2e8f0;--muted:#64748b;--blue:#38bdf8;--blue-dim:#0c4a6e;--green:#4ade80;--green-dim:#14532d;--red:#f87171;--red-dim:#7f1d1d;--r-t:#38bdf8;--r-d:#a78bfa;--r-e:#34d399;--radius:14px;}
    *,*::before,*::after{box-sizing:border-box;margin:0;padding:0;}
    html{font-size:16px;-webkit-tap-highlight-color:transparent;}
    body{background:var(--bg);color:var(--text);font-family:system-ui,sans-serif;min-height:100dvh;padding:0 0 80px;overflow-x:hidden;}
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
    input[type="text"]{width:100%;padding:13px 14px;background:var(--bg);border:1px solid var(--border);border-radius:10px;color:var(--text);font-size:16px;font-weight:600;outline:none;transition:border-color .2s;-webkit-appearance:none;}
    input[type="text"]:focus{border-color:var(--blue);}
    input[type="text"]::placeholder{color:var(--muted);font-weight:400;}
    .btn{display:flex;align-items:center;justify-content:center;gap:8px;width:100%;padding:15px;border:none;border-radius:12px;font-size:15px;font-weight:700;cursor:pointer;transition:opacity .15s,transform .1s;-webkit-appearance:none;}
    .btn:active{transform:scale(.97);opacity:.85;}
    .btn:disabled{opacity:.3;cursor:not-allowed;transform:none;}
    .btn+.btn{margin-top:8px;}
    .btn-iniciar{background:var(--blue);color:#000;}
    .btn-encerrar{background:var(--red);color:#fff;}
    .btn-csv{background:var(--green);color:#000;}
    .btn-outline{background:transparent;border:1px solid var(--border);color:var(--muted);}
    .regioes-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;}
    .regiao-card{background:var(--bg);border:1px solid var(--border);border-radius:12px;padding:14px 10px;text-align:center;transition:border-color .3s,background .3s;}
    .regiao-card.flash{background:rgba(56,189,248,.12);border-color:var(--blue);}
    .regiao-card .r-icon{font-size:22px;display:block;margin-bottom:6px;}
    .regiao-card .r-nome{font-size:10px;font-family:monospace;color:var(--muted);display:block;margin-bottom:8px;line-height:1.3;}
    .regiao-card .r-count{font-size:28px;font-weight:800;display:block;}
    #card-traqueia .r-count{color:var(--r-t);}
    #card-bronquio_direito .r-count{color:var(--r-d);}
    #card-bronquio_esquerdo .r-count{color:var(--r-e);}
    #log-lista{list-style:none;display:flex;flex-direction:column;gap:6px;max-height:260px;overflow-y:auto;}
    #log-lista:empty::after{content:"Nenhuma detecção ainda…";display:block;text-align:center;color:var(--muted);font-family:monospace;font-size:13px;padding:20px 0;}
    .log-item{display:flex;align-items:center;gap:10px;padding:10px 12px;background:var(--bg);border-radius:8px;border-left:3px solid var(--border);animation:slide-in .25s ease;}
    @keyframes slide-in{from{opacity:0;transform:translateX(-10px);}to{opacity:1;transform:translateX(0);}}
    .log-item.t{border-color:var(--r-t);}
    .log-item.d{border-color:var(--r-d);}
    .log-item.e{border-color:var(--r-e);}
    .log-item .l-num{font-family:monospace;font-size:11px;color:var(--muted);min-width:22px;}
    .log-item .l-nome{flex:1;font-size:13px;font-weight:600;}
    .log-item .l-hora{font-family:monospace;font-size:11px;color:var(--muted);}
    #aluno-ativo-banner{display:none;align-items:center;gap:10px;background:var(--blue-dim);border:1px solid var(--blue);border-radius:10px;padding:12px 14px;margin-bottom:12px;}
    #aluno-ativo-banner.visible{display:flex;}
    .ab-label{font-size:11px;color:var(--blue);font-family:monospace;}
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
    <span>Centro Cirúrgico · v2.0</span>
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
      <div class="ab-label">TESTE EM ANDAMENTO</div>
      <div class="ab-nome" id="banner-nome-aluno">—</div>
    </div>
  </div>
  <div id="timer">⏱ Tempo: <span id="timer-valor">00:00</span></div>
  <div class="card">
    <div class="card-title">▸ Detecções por Região</div>
    <div class="regioes-grid">
      <div class="regiao-card" id="card-traqueia">
        <span class="r-icon">🔵</span>
        <span class="r-nome">Traqueia</span>
        <span class="r-count" id="count-traqueia">0</span>
      </div>
      <div class="regiao-card" id="card-bronquio_direito">
        <span class="r-icon">🟣</span>
        <span class="r-nome">Brônq. Direito</span>
        <span class="r-count" id="count-bronquio_direito">0</span>
      </div>
      <div class="regiao-card" id="card-bronquio_esquerdo">
        <span class="r-icon">🟢</span>
        <span class="r-nome">Brônq. Esquerdo</span>
        <span class="r-count" id="count-bronquio_esquerdo">0</span>
      </div>
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
    ["traqueia","bronquio_direito","bronquio_esquerdo"].forEach(r=>{
      const c=data.deteccoes.filter(d=>d.regiao===r).length;
      document.getElementById("count-"+r).textContent=c;
    });
    if(data.total>totalAntes){
      const novas=data.deteccoes.slice(totalAntes);
      novas.forEach((det,i)=>adicionarLogItem(det,totalAntes+i+1));
      flashRegiao(novas[novas.length-1].regiao);
      totalAntes=data.total;
    }
  }
  function flashRegiao(r){
    const c=document.getElementById("card-"+r);
    if(!c)return;
    c.classList.add("flash");
    setTimeout(()=>c.classList.remove("flash"),600);
  }
  function adicionarLogItem(det,num){
    const lista=document.getElementById("log-lista");
    const li=document.createElement("li");
    const cor=det.regiao==="traqueia"?"t":det.regiao==="bronquio_direito"?"d":"e";
    const nome=det.regiao==="traqueia"?"Traqueia":det.regiao==="bronquio_direito"?"Brônquio Direito":"Brônquio Esquerdo";
    li.className="log-item "+cor;
    li.innerHTML=`<span class="l-num">#${num}</span><span class="l-nome">${nome}</span><span class="l-hora">${det.timestamp}</span>`;
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
      ["traqueia","bronquio_direito","bronquio_esquerdo"].forEach(r=>document.getElementById("count-"+r).textContent="0");
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
        const m=String(Math.floor(s/60)).padStart(2,"0");
        const sc=String(s%60).padStart(2,"0");
        document.getElementById("timer-valor").textContent=m+":"+sc;
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
      if(!d.ok){mostrarToast(d.erro||"Erro ao encerrar.","err");return;}
      await consultarEstado();
      pararPolling();clearInterval(timerInterval);
      testeAtivo=false;
      document.getElementById("status-badge").textContent="ENCERRADO";
      document.getElementById("status-badge").classList.remove("ativo");
      document.getElementById("btn-encerrar").disabled=true;
      document.getElementById("btn-csv").disabled=false;
      mostrarToast("🛑 Teste encerrado — "+d.total_deteccoes+" detecção(ões)","ok");
    }catch(e){mostrarToast("Sem conexão com o ESP32!","err");}
  }
  function baixarCSV(){window.location.href="/relatorio";mostrarToast("⬇ Baixando CSV…","ok");}
  function limparTela(){
    if(testeAtivo){mostrarToast("Encerre o teste antes.","err");return;}
    document.getElementById("log-lista").innerHTML="";
    ["traqueia","bronquio_direito","bronquio_esquerdo"].forEach(r=>document.getElementById("count-"+r).textContent="0");
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
</html>
)rawhtml";

  server.send(200, "text/html; charset=utf-8", html);
}


/* ================================================================
   ROTA: /iniciar — Inicia novo teste
   ================================================================ */
void handleIniciar() {
  server.sendHeader("Access-Control-Allow-Origin", "*");

  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"erro\":\"Body ausente.\"}");
    return;
  }

  String body = server.arg("plain");
  // Extrai o nome do JSON manualmente (sem biblioteca extra)
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

  Serial.println("Teste INICIADO para: " + nomeAluno);
  server.send(200, "application/json", "{\"ok\":true,\"mensagem\":\"Teste iniciado para " + nomeAluno + "\"}");
}


/* ================================================================
   ROTA: /encerrar — Encerra o teste
   ================================================================ */
void handleEncerrar() {
  server.sendHeader("Access-Control-Allow-Origin", "*");

  if (!testeAtivo) {
    server.send(400, "application/json", "{\"ok\":false,\"erro\":\"Nenhum teste ativo.\"}");
    return;
  }

  testeAtivo = false;
  Serial.println("Teste ENCERRADO para: " + nomeAluno);
  server.send(200, "application/json",
    "{\"ok\":true,\"mensagem\":\"Teste encerrado\",\"total_deteccoes\":" + String(totalDeteccoes) + "}");
}


/* ================================================================
   ROTA: /estado — Retorna estado atual (polling)
   ================================================================ */
void handleEstado() {
  server.sendHeader("Access-Control-Allow-Origin", "*");

  String json = "{";
  json += "\"ativa\":" + String(testeAtivo ? "true" : "false") + ",";
  json += "\"aluno\":\"" + nomeAluno + "\",";
  json += "\"inicio\":\"" + inicioTeste + "\",";
  json += "\"total\":" + String(totalDeteccoes) + ",";
  json += "\"deteccoes\":[";

  for (int i = 0; i < totalDeteccoes; i++) {
    if (i > 0) json += ",";
    json += "{\"regiao\":\"" + deteccoes[i].regiao + "\",\"timestamp\":\"" + deteccoes[i].timestamp + "\"}";
  }

  json += "]}";
  server.send(200, "application/json", json);
}


/* ================================================================
   ROTA: /relatorio — Gera e envia o CSV
   ================================================================ */
void handleRelatorio() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Content-Disposition", "attachment; filename=relatorio_" + nomeAluno + ".csv");

  String csv = "SIMULADOR DE TREINAMENTO BRONQUICO\n";
  csv += "Aluno," + nomeAluno + "\n";
  csv += "Inicio," + inicioTeste + "\n";
  csv += "Total de Deteccoes," + String(totalDeteccoes) + "\n\n";

  // Resumo por região
  csv += "RESUMO POR REGIAO\n";
  int cT = 0, cD = 0, cE = 0;
  for (int i = 0; i < totalDeteccoes; i++) {
    if (deteccoes[i].regiao == "traqueia")          cT++;
    else if (deteccoes[i].regiao == "bronquio_direito")  cD++;
    else if (deteccoes[i].regiao == "bronquio_esquerdo") cE++;
  }
  csv += "Traqueia," + String(cT) + "\n";
  csv += "Bronquio Direito," + String(cD) + "\n";
  csv += "Bronquio Esquerdo," + String(cE) + "\n\n";

  // Log detalhado
  csv += "LOG DETALHADO\n";
  csv += "#,Regiao,Timestamp\n";
  for (int i = 0; i < totalDeteccoes; i++) {
    csv += String(i + 1) + "," + deteccoes[i].regiao + "," + deteccoes[i].timestamp + "\n";
  }

  server.send(200, "text/csv", csv);
}


/* ================================================================
   FUNÇÃO: piscarLED
   ================================================================ */
void piscarLED() {
  digitalWrite(PINO_LED, HIGH);
  delay(300);
  digitalWrite(PINO_LED, LOW);
}
