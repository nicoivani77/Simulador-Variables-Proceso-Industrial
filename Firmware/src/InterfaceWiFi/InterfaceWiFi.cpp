/*
 * Proyecto: Simulador de Variables de Proceso para Lazos de Control Industrial
 * Version: v1.5
 * Autor: Nicolas Emiliano Ivani
 * Descripcion: Implementación de la interfaz web embebida y los servicios HTTP de configuración y monitoreo.
 */

#include "InterfaceWiFi.h"
#include <esp_system.h>
#include <SD.h>
#include <math.h>

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Simulador de Variables de Proceso para Lazos de Control Industrial</title>
  <style>
    :root{
      --bg:#0b1120;--bg2:#0f172a;--sidebar:#0f172a;--panel:#111827;--panel2:#0b1220;
      --text:#e5e7eb;--muted:#94a3b8;--soft:#cbd5e1;--border:#253247;
      --accent:#38bdf8;--accent2:#0ea5e9;--input:#f59e0b;--ok:#22c55e;--warn:#f97316;--fault:#ef4444;
      --shadow:0 18px 45px rgba(0,0,0,.28);--radius:18px;
    }
    *{box-sizing:border-box}
    html{scroll-behavior:smooth}
    body{margin:0;font-family:Inter,Segoe UI,Arial,Helvetica,sans-serif;background:radial-gradient(circle at top left,#172554 0,#0b1120 28%,#0f172a 100%);color:var(--text)}
    button,input,select{font:inherit}
    .app{min-height:100vh;display:block}
    .sidebar{position:fixed;top:0;left:0;z-index:40;width:280px;height:100vh;padding:18px;background:rgba(15,23,42,.96);border-right:1px solid var(--border);backdrop-filter:blur(12px);display:flex;flex-direction:column;gap:16px;transition:transform .18s ease;overflow:auto}
    .content{min-width:0;margin-left:280px;padding:20px 24px 28px;transition:margin-left .18s ease}
    body.sidebar-collapsed .sidebar{transform:translateX(-105%)}
    body.sidebar-collapsed .content{margin-left:0}
    .scrim{display:none;position:fixed;inset:0;background:rgba(2,6,23,.58);z-index:35}
    .brand{padding:16px;border:1px solid var(--border);border-radius:22px;background:linear-gradient(180deg,rgba(56,189,248,.12),rgba(15,23,42,.7));box-shadow:var(--shadow)}
    .brandTitle{font-size:1.15rem;font-weight:800;letter-spacing:.2px}.brandSub{margin-top:8px;color:var(--muted);font-size:.88rem;line-height:1.35}
    .navGroupTitle{margin:4px 6px 2px;color:#64748b;text-transform:uppercase;font-size:.72rem;font-weight:800;letter-spacing:.08em}
    .nav{display:flex;flex-direction:column;gap:6px}
    .navBtn{width:100%;border:1px solid transparent;background:transparent;color:var(--soft);text-align:left;padding:11px 12px;border-radius:14px;cursor:pointer;font-weight:700;display:flex;align-items:center;justify-content:flex-start;gap:10px}
    .navBtn:hover{background:rgba(148,163,184,.12);border-color:rgba(148,163,184,.18)}
    .navBtn.active{background:rgba(56,189,248,.16);border-color:rgba(56,189,248,.45);color:#e0f2fe}
    .navIcon{width:24px;min-width:24px;text-align:left;opacity:.95}
    .topbar{display:flex;justify-content:space-between;align-items:flex-start;gap:16px;margin-bottom:18px}.topbarLeft{display:flex;align-items:flex-start;gap:12px;min-width:0}.menuToggle{margin-top:0;min-width:44px;height:42px;padding:0 12px;background:#0b1220;color:#e0f2fe;border:1px solid var(--border);font-size:1.35rem;line-height:1}.pageTitle{font-size:1.8rem;font-weight:850;letter-spacing:-.03em}.pageSubtitle{margin-top:6px;color:var(--muted);line-height:1.45}.statusPills{display:flex;flex-wrap:wrap;justify-content:flex-end;gap:8px}.pill{padding:8px 11px;border-radius:999px;border:1px solid var(--border);background:rgba(11,18,32,.85);font-size:.88rem;color:var(--soft)}
    .view{display:none;animation:fade .16s ease-out}.view.active{display:block}@keyframes fade{from{opacity:.4;transform:translateY(4px)}to{opacity:1;transform:none}}
    .grid{display:grid;gap:16px}.grid.two{grid-template-columns:minmax(340px,1.05fr) minmax(320px,.95fr)}.grid.two.dashboardMain{grid-template-columns:minmax(0,calc(75% - 4px)) minmax(0,calc(25% - 12px))}.grid.three{grid-template-columns:repeat(3,minmax(0,1fr))}.grid.kpi{grid-template-columns:repeat(4,minmax(0,1fr));margin-bottom:16px}.fullSpan{grid-column:1/-1}
    .card,.kpiCard{background:rgba(17,24,39,.96);border:1px solid var(--border);border-radius:var(--radius);padding:16px;box-shadow:var(--shadow)}
    .card h2{margin:0 0 12px;font-size:1.12rem}.card h3{margin:14px 0 8px;font-size:1rem;color:#cbd5e1}.sectionLead{margin:-4px 0 14px;color:var(--muted);line-height:1.45;font-size:.94rem}
    .kpiCard{min-height:112px;display:flex;flex-direction:column;justify-content:space-between}.kpiCard.kpiEqual{min-height:132px}.kpiCard.compact{gap:8px}.kpiContent{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;text-align:center;gap:5px}.kpiLabel{color:var(--muted);font-size:.86rem}.kpiValue{font-size:1.5rem;font-weight:850;letter-spacing:-.02em}.kpiHint{color:#64748b;font-size:.82rem}.captureActions{display:flex;gap:8px;align-items:center;justify-content:center;flex-wrap:wrap}.captureActions button,.captureActions a{margin-top:0}.lastCsvCompact{max-width:100%;font-size:.78rem;color:var(--muted);overflow:hidden;text-overflow:ellipsis;white-space:nowrap;text-align:center}.modeContent{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:9px;text-align:center}.modeActiveCheck{width:fit-content;justify-content:center;padding:7px 10px;border-radius:11px}.modeControls{display:flex;gap:8px;align-items:center;justify-content:center;flex-wrap:wrap}.modeControls .check{padding:7px 9px;border-radius:11px;white-space:nowrap}.modeControls input[type=number]{width:74px;margin:0;padding:7px 8px;text-align:center}.recording{animation:pulseRec 1.15s ease-in-out infinite;box-shadow:0 0 0 0 rgba(239,68,68,.45)}@keyframes pulseRec{0%{box-shadow:0 0 0 0 rgba(239,68,68,.5)}70%{box-shadow:0 0 0 8px rgba(239,68,68,0)}100%{box-shadow:0 0 0 0 rgba(239,68,68,0)}}
    .metricRows{display:flex;flex-direction:column;gap:10px}.row{display:flex;justify-content:space-between;align-items:center;gap:12px;margin:0}.row.summaryStackRow,.row.operationalFaults{align-items:flex-start}.label{color:var(--muted);flex:0 0 auto}.value{font-weight:800;text-align:right}.value.big{font-size:1.2rem}.value.fault{color:var(--fault)}.operationalSummary{--summaryValueCol:156px}.operationalSummary .metricRows{gap:10px}.operationalSummary .row{display:grid;grid-template-columns:max-content var(--summaryValueCol);justify-content:space-between;align-items:flex-start;gap:24px}.operationalSummary .value{text-align:left;justify-self:stretch;min-width:0}.summaryStack{display:flex;flex-direction:column;gap:4px;align-items:flex-start;text-align:left;min-width:0;width:100%}.summaryStackLine{line-height:1.25;white-space:nowrap}.summaryAttentionManual{color:var(--warn);font-weight:900}.summaryAttentionInactive{color:var(--fault);font-weight:900}.faultList{display:flex;flex-direction:column;gap:4px;align-items:flex-start;text-align:left;width:100%}.faultItem{line-height:1.25;white-space:nowrap}.faultItemValue{color:#fecaca;font-weight:800}.faultOk{color:var(--ok);font-weight:850}
    .legend{display:flex;gap:12px;flex-wrap:wrap;margin:10px 0 6px}.dot{display:inline-block;width:10px;height:10px;border-radius:999px;margin-right:6px}.dot.out{background:var(--accent)}.dot.in{background:var(--input)}
    .chartBox{margin-top:12px;background:#070d1a;border:1px solid var(--border);border-radius:16px;padding:10px}canvas{width:100%;height:285px;display:block}
    input,select{width:100%;padding:10px 12px;margin:6px 0 10px;border-radius:12px;border:1px solid var(--border);background:#090f1d;color:var(--text);outline:none}input:focus,select:focus{border-color:var(--accent);box-shadow:0 0 0 3px rgba(56,189,248,.13)}input:disabled{opacity:.62;cursor:not-allowed}input[type=checkbox]{width:auto;margin:0 8px 0 0;padding:0;accent-color:var(--accent)}.hiddenField{display:none!important}
    button,.downloadButton{margin-top:8px;padding:10px 14px;border:0;border-radius:12px;cursor:pointer;font-weight:800;background:var(--accent);color:#082f49;transition:transform .08s ease,filter .12s ease;text-decoration:none;display:inline-flex;align-items:center;justify-content:center;gap:8px}button:hover,.downloadButton:hover{filter:brightness(1.06)}button:active,.downloadButton:active{transform:translateY(1px)}.btn-secondary{background:#334155;color:#e5e7eb}.btn-warn{background:var(--warn);color:#111827}.btn-danger{background:var(--fault);color:white}.btn-ghost,.downloadButton{background:transparent;color:var(--soft);border:1px solid var(--border)}.btn-play{background:var(--ok);color:#052e16}.iconButton,.downloadButton{width:46px;min-width:46px;padding:10px 0;font-size:1.05rem}.btnIcon{width:38px;min-width:38px;height:38px;margin-top:0;padding:0;border:0;border-radius:12px;display:flex;align-items:center;justify-content:center;font-size:.82rem;font-weight:850;background:#334155;color:#e5e7eb;cursor:pointer;text-decoration:none;letter-spacing:.02em;transition:transform .08s ease,filter .12s ease,opacity .12s ease}.btnIcon:hover{filter:brightness(1.06)}.btnIcon:active{transform:translateY(1px)}.btnIcon:disabled{opacity:.45;cursor:not-allowed;filter:none;transform:none}.btnIcon.rec{background:var(--fault);color:white}.btnIcon.stop{background:#334155;color:#fca5a5}.btnIcon.stop.active{background:var(--fault);color:white}.btnIcon.download{background:transparent;color:var(--soft);border:1px solid var(--border)}
    .small,.foot{color:var(--muted);font-size:.9rem}.small{margin-top:10px;line-height:1.45}.rangeHint{display:block;margin:-5px 0 8px;color:var(--muted);font-size:.78rem;font-weight:500}.foot{margin-top:18px;text-align:center;color:#93c5fd;font-size:.9rem}.authorLink{position:relative;color:var(--accent);font-weight:850;text-decoration:underline;text-decoration-thickness:1px;text-underline-offset:3px;cursor:pointer}.authorLink:hover{color:#7dd3fc}.authorLink::after{content:attr(data-tooltip);position:absolute;left:50%;bottom:calc(100% + 9px);transform:translateX(-50%) translateY(4px);padding:6px 9px;border-radius:9px;border:1px solid var(--border);background:#020617;color:var(--text);font-size:.78rem;font-weight:700;white-space:nowrap;opacity:0;pointer-events:none;transition:opacity .14s ease,transform .14s ease;box-shadow:var(--shadow);z-index:60}.authorLink:hover::after,.authorLink:focus-visible::after{opacity:1;transform:translateX(-50%) translateY(0)}.authorLink:focus-visible{outline:2px solid var(--accent);outline-offset:3px;border-radius:4px}.formGrid{display:grid;grid-template-columns:1fr 1fr;gap:10px}.formGrid.threeInline{grid-template-columns:repeat(3,minmax(0,1fr))}.formGrid.processInline{grid-template-columns:repeat(6,minmax(0,1fr));align-items:end}.formGrid.processInline input{min-width:0}.formGrid.processInline .check{height:42px;margin:26px 0 10px}.formGrid .full{grid-column:1/-1}.btnRow{display:flex;flex-wrap:wrap;gap:8px;align-items:center}.btnRow button,.btnRow .downloadButton{margin-top:8px}.checkboxGrid{display:grid;grid-template-columns:1fr;gap:8px;margin-top:10px}.checkboxGrid.twoCols{grid-template-columns:1fr 1fr}.checkboxGrid.threeCols{grid-template-columns:repeat(3,minmax(0,1fr))}.check{display:flex;align-items:center;padding:10px 11px;border:1px solid var(--border);border-radius:13px;background:#090f1d;color:var(--text)}
    .configHeader{display:flex;justify-content:space-between;align-items:flex-start;gap:14px;margin-bottom:14px}.configHeader h2{margin:0}.configHeader p{margin:6px 0 0;color:var(--muted);line-height:1.4}.actionBar{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}.collapsibleModel{margin-top:16px;border:1px solid var(--border);border-radius:var(--radius);background:rgba(17,24,39,.96);box-shadow:var(--shadow);overflow:hidden}.collapsibleModel summary{list-style:none;cursor:pointer;padding:15px 16px;display:flex;align-items:center;justify-content:space-between;gap:12px;font-weight:850}.collapsibleModel summary::-webkit-details-marker{display:none}.collapsibleModel summary::after{content:'▾';color:var(--muted);transition:transform .16s ease}.collapsibleModel[open] summary::after{transform:rotate(180deg)}.summaryText{display:flex;flex-direction:column;gap:4px}.summaryTitle{font-size:1.05rem}.summarySub{font-size:.9rem;color:var(--muted);font-weight:500}.modelDetailsBody{padding:0 16px 16px}.toast{position:fixed;right:20px;bottom:20px;max-width:min(420px,calc(100vw - 40px));padding:13px 15px;border-radius:14px;border:1px solid rgba(56,189,248,.35);background:rgba(8,47,73,.96);color:#e0f2fe;box-shadow:var(--shadow);display:none;z-index:50}.toast.show{display:block;animation:fade .16s ease-out}
    @media(max-width:1050px){.sidebar{transform:translateX(-105%);width:min(86vw,280px)}body.sidebar-open .sidebar{transform:translateX(0)}body.sidebar-open .scrim{display:block}.content,body.sidebar-collapsed .content{margin-left:0;padding:16px}.topbar{flex-direction:column}.statusPills{justify-content:flex-start}.grid.two,.grid.two.dashboardMain,.grid.three,.grid.kpi{grid-template-columns:1fr 1fr}}
    @media(max-width:680px){.grid.two,.grid.two.dashboardMain,.grid.three,.grid.kpi,.formGrid,.formGrid.threeInline,.formGrid.processInline,.checkboxGrid.twoCols,.checkboxGrid.threeCols{grid-template-columns:1fr}.pageTitle{font-size:1.45rem}canvas{height:230px}.configHeader{flex-direction:column}.actionBar{justify-content:flex-start}.topbarLeft{width:100%}.menuToggle{position:sticky;top:10px;z-index:30}.kpiCard{min-height:96px}}
  </style>
</head>
<body>
  <div class="app">
    <aside class="sidebar">
      <div class="brand">
        <div class="brandTitle" id="deviceName">Simulador de Variables de Proceso para Lazos de Control Industrial</div>
        <div class="brandSub">Panel de operación y configuración WiFi para el simulador 4-20 mA.</div>
      </div>
      <div>
        <div class="navGroupTitle">Navegación</div>
        <nav class="nav">
          <button class="navBtn active" data-view="dashboard"><span class="navIcon">📈</span>Visualización</button>
          <button class="navBtn" data-view="system"><span class="navIcon">🧰</span>Sistema</button>
          <button class="navBtn" data-view="calibration"><span class="navIcon">📐</span>Calibración</button>
          <button class="navBtn" data-view="about"><span class="navIcon">👤</span>Acerca de</button>
        </nav>
      </div>
    </aside>
    <div class="scrim" onclick="closeSidebar()"></div>

    <main class="content">
      <header class="topbar">
        <div class="topbarLeft">
          <button class="menuToggle" onclick="toggleSidebar()" aria-label="Abrir navegación">☰</button>
          <div>
            <div class="pageTitle" id="viewTitle">Operación y visualización</div>
            <div class="pageSubtitle" id="viewSubtitle">Pantalla principal para operar el equipo y observar variables, tendencia y estado del lazo.</div>
          </div>
        </div>
        <div class="statusPills">
          <div class="pill" id="wifiSsid">Red: --</div>
        </div>
      </header>

      <section class="view active" id="view-dashboard">
        <div class="grid kpi">
          <div class="kpiCard kpiEqual">
            <div class="kpiLabel">Entrada medida</div>
            <div class="kpiContent">
              <div class="kpiValue" id="inputPct">--</div>
              <div class="kpiHint">Desde AO del PLC</div>
            </div>
          </div>
          <div class="kpiCard kpiEqual">
            <div class="kpiLabel">Salida generada</div>
            <div class="kpiContent">
              <div class="kpiValue" id="outputPct">--</div>
              <div class="kpiHint">Hacia AI del PLC</div>
            </div>
          </div>
          <div class="kpiCard compact modeCard">
            <div class="kpiLabel">Modo</div>
            <div class="modeContent">
              <label class="check modeActiveCheck"><input type="checkbox" id="outputEnabled">Activar salida</label>
              <div class="modeControls">
                <label class="check"><input type="checkbox" id="modeAuto">Auto</label>
                <label class="check"><input type="checkbox" id="modeManual">Manual</label>
                <input id="manualOutputPct" type="number" step="1" min="0" max="100" aria-label="Salida manual [%]">
              </div>
            </div>
            <input type="checkbox" id="manualOutputMode" class="hiddenField" aria-hidden="true" tabindex="-1">
          </div>
          <div class="kpiCard kpiEqual compact">
            <div class="kpiLabel">Captura</div>
            <input type="checkbox" id="loggingEnabled" class="hiddenField" aria-hidden="true" tabindex="-1">
            <div class="kpiContent">
              <div class="captureActions">
                <button id="captureRec" class="btnIcon rec" onclick="setCaptureEnabled(true)" title="Iniciar captura">REC</button>
                <button id="captureStop" class="btnIcon stop" onclick="setCaptureEnabled(false)" title="Detener captura">■</button>
                <a class="btnIcon download" id="downloadCsv" href="/api/log/download" download title="Descargar último CSV">⬇</a>
              </div>
              <div class="lastCsvCompact">Último CSV: <span id="lastCsvInfo">--</span></div>
              <div class="lastCsvCompact">Estado SD: <span id="sdCaptureInfo">--</span></div>
            </div>
          </div>
        </div>
        <div class="grid two dashboardMain">
          <div class="card">
            <h2>Tendencia en tiempo real</h2>
            <div class="legend"><span><span class="dot in"></span>Entrada</span><span><span class="dot out"></span>Salida</span></div>
            <div class="chartBox"><canvas id="trend"></canvas></div>
          </div>
          <div class="card operationalSummary">
            <h2>Resumen operativo</h2>
            <div class="metricRows">
              <div class="row"><span class="label">Preset</span><span class="value" id="presetInfo">--</span></div>
              <div class="row summaryStackRow"><span class="label">Salida</span><span class="value summaryStack" id="outputStateInfo">--</span></div>
              <div class="row summaryStackRow"><span class="label">Modelo</span><span class="value summaryStack" id="modelInfo">--</span></div>
              <div class="row summaryStackRow operationalFaults"><span class="label">Alteraciones</span><span class="value faultList summaryStack" id="faultInfo">--</span></div>
            </div>
          </div>
        </div>
        <details class="collapsibleModel">
          <summary>
            <span class="summaryText"><span class="summaryTitle">Modelo de proceso</span><span class="summarySub">Parámetros dinámicos, presets y sensor.</span></span>
          </summary>
          <div class="modelDetailsBody">
            <div class="card">
              <label>Preset de proceso
                <select id="preset">
                  <option value="1">Nivel</option>
                  <option value="2">Caudal</option>
                  <option value="3">Presión</option>
                  <option value="4">Temperatura</option>
                  <option value="0">Personalizado</option>
                </select>
              </label>
              <div class="formGrid processInline">
                <label>Ganancia (K)<input id="procK" type="number" step="0.05" min="0" max="5"><span class="rangeHint">0.0 - 5.0</span></label>
                <label>Tau (τ) [s]<input id="procTau" type="number" step="0.1" min="0.1" max="60"><span class="rangeHint">0.1 - 60.0 s</span></label>
                <label>Retardo (θ) [s]<input id="procDead" type="number" step="0.1" min="0" max="30"><span class="rangeHint">0.0 - 30.0 s</span></label>
                <label>Ruido de medición ± [%]<input id="sensorNoise" type="number" step="0.1" min="0" max="10"><span class="rangeHint">0.0 - 10.0 %</span></label>
                <label>Offset [%]<input id="sensorOffset" type="number" step="0.5" min="-25" max="25"><span class="rangeHint">-25.0 - +25.0 %</span></label>
                <label class="check"><input type="checkbox" id="sensorFreeze">Congelar variable</label>
              </div>
              <div class="small">K define la ganancia estática del proceso. Tau (τ) define la constante de tiempo. Retardo desplaza el inicio de la respuesta.</div>
              <div class="small">Al editar K, Tau o Retardo, el preset pasa a Personalizado. Ruido de medición y Offset se aplican automáticamente cuando su valor es distinto de cero.</div>
            </div>
          </div>
        </details>
      </section>

      <section class="view" id="view-system">
        <div class="grid two">
          <div class="card">
            <h2>Operación y servicios</h2>
            <div class="checkboxGrid">
              <label class="check"><input type="checkbox" id="audioEnabled">Audio</label>
            </div>
            <div class="btnRow"><button class="btn-danger" onclick="restartDevice()">Reiniciar equipo</button></div>
          </div>
          <div class="card">
            <h2>Tarjeta SD</h2>
            <div class="metricRows">
              <div class="row"><span class="label">Estado SD</span><span class="value" id="sdStateMirror">--</span></div>
              <div class="row"><span class="label">Último CSV</span><span class="value" id="lastCsvSystem">--</span></div>
            </div>
            <div class="btnRow"><button class="btn-secondary" onclick="sendAction('sdDetect')">Detectar SD</button></div>
          </div>
          <div class="card fullSpan">
            <h2>Columnas CSV</h2>
            <div class="checkboxGrid threeCols">
              <label class="check"><input type="checkbox" id="logMs" data-log-bit="1">Tiempo [ms]</label>
              <label class="check"><input type="checkbox" id="logInputPct" data-log-bit="4">Entrada [%]</label>
              <label class="check"><input type="checkbox" id="logOutputPct" data-log-bit="16">Salida [%]</label>
              <label class="check"><input type="checkbox" id="logOutputEnabled" data-log-bit="32">Salida activa</label>
              <label class="check"><input type="checkbox" id="logMode" data-log-bit="64">Modo</label>
              <label class="check"><input type="checkbox" id="logPreset" data-log-bit="128">Preset</label>
              <label class="check"><input type="checkbox" id="logK" data-log-bit="256">Ganancia (K)</label>
              <label class="check"><input type="checkbox" id="logTau" data-log-bit="512">Tau (τ) [s]</label>
              <label class="check"><input type="checkbox" id="logDeadTime" data-log-bit="1024">Retardo (θ) [s]</label>
              <label class="check"><input type="checkbox" id="logNoise" data-log-bit="2048">Ruido de medición [%]</label>
              <label class="check"><input type="checkbox" id="logOffset" data-log-bit="4096">Offset [%]</label>
              <label class="check"><input type="checkbox" id="logErrors" data-log-bit="8192">Estado / errores</label>
              <label class="check"><input type="checkbox" id="logCalibrationOutput" data-log-bit="16384">Salida en calibración</label>
            </div>
            <div class="small" id="logInfo">Columnas: --</div>
            <div class="small">Si cambiás columnas durante una captura, el firmware cierra el archivo actual y abre uno nuevo para que el encabezado no quede mezclado.</div>
            <div class="actionBar"><button class="btn-secondary" onclick="saveLogConfig()">Guardar columnas CSV</button></div>
          </div>
          <div class="card fullSpan">
            <h2>WiFi</h2>
            <div class="formGrid">
              <input id="ssid" placeholder="SSID de la red local">
              <input id="pass" placeholder="Clave" type="password">
            </div>
            <div class="btnRow">
              <button class="btn-secondary" onclick="saveWiFi()">Guardar WiFi</button>
              <button class="btn-warn" onclick="sendAction('wifiReset')">Borrar WiFi guardado</button>
            </div>
            <div class="small">Si las credenciales fallan, el equipo levanta su Access Point de configuración.</div>
          </div>
        </div>
      </section>

      <section class="view" id="view-calibration">
        <div class="grid two">
          <div class="card">
            <h2>Entrada analógica (<span id="inputRawCurrent">--</span>)</h2>
            <div class="formGrid">
              <label>RAW 4 mA<input id="inputRaw4" type="number" disabled></label>
              <label>RAW 20 mA<input id="inputRaw20" type="number" disabled></label>
            </div>
            <div class="btnRow">
              <button class="btn-secondary" onclick="sendAction('calInput4')">Tomar entrada 4 mA</button>
              <button class="btn-secondary" onclick="sendAction('calInput20')">Tomar entrada 20 mA</button>
            </div>
          </div>
          <div class="card">
            <h2>Salida DAC (<span id="outputRawCurrent">--</span>)</h2>
            <div class="formGrid">
              <label>RAW 4 mA<input id="outputRaw4" type="number" step="1" min="0" max="4095"><span class="calCurrent" id="outputRaw4Actual">Actual: --</span></label>
              <label>RAW 20 mA<input id="outputRaw20" type="number" step="1" min="0" max="4095"><span class="calCurrent" id="outputRaw20Actual">Actual: --</span></label>
            </div>
            <div class="btnRow">
              <button class="btn-secondary" onclick="sendCalibrationTest('testOut4')">Probar 4 mA</button>
              <button class="btn-secondary" onclick="sendCalibrationTest('testOut20')">Probar 20 mA</button>
              <button onclick="saveOutputCalibration()">Guardar RAW salida</button>
            </div>
            <div class="small">Los botones de prueba aplican temporalmente los valores escritos durante 3 segundos, sin guardarlos. Usalos con el lazo en condición segura.</div>
          </div>
        </div>
      </section>

      <section class="view" id="view-about">
        <div class="grid two">
          <div class="card">
            <h2>Acerca del proyecto</h2>
            <p class="sectionLead">Simulador de Variables de Proceso para Lazos de Control Industrial, desarrollado sobre ESP32-S3 para representar la dinámica de un proceso, operar un lazo de control y registrar sus principales variables.</p>
            <div class="metricRows">
              <div class="row"><span class="label">Proyecto</span><span class="value">Simulador de Variables de Proceso para Lazos de Control Industrial</span></div>
              <div class="row"><span class="label">Versión de interfaz</span><span class="value">v1.5</span></div>
              <div class="row"><span class="label">Autor</span><span class="value"><a href="#" class="authorLink" data-tooltip="Hello there" title="Hello there" onclick="triggerAuthorDetail(event)">Nicolas Emiliano Ivani</a></span></div>
            </div>
          </div>
          <div class="card">
            <h2>Alcance</h2>
            <p class="sectionLead">El sistema recibe la señal de mando del PLC, calcula la respuesta del modelo de proceso y devuelve la variable simulada mediante una señal analógica de 4-20 mA. Además permite configurar el modelo y registrar datos para análisis posterior.</p>
            <div class="metricRows">
              <div class="row"><span class="label">Entrada</span><span class="value">Señal 4-20 mA desde la AO del PLC</span></div>
              <div class="row"><span class="label">Salida</span><span class="value">Señal 4-20 mA hacia la AI del PLC</span></div>
              <div class="row"><span class="label">Registro</span><span class="value">CSV configurable en tarjeta SD</span></div>
            </div>
          </div>
        </div>
      </section>

      <div class="foot">Simulador de Variables de Proceso para Lazos de Control Industrial v1.5 - Nicolas Emiliano Ivani</div>
    </main>
  </div>

  <div class="toast" id="toast"></div>

  <script>
    const MAX_POINTS = 90;
    const hist = { input: [], output: [] };
    const PRESETS = {
      0:{K:1.00,tau:2.0,deadTime:0.0},
      1:{K:1.00,tau:10.0,deadTime:0.8},
      2:{K:1.00,tau:2.0,deadTime:0.2},
      3:{K:1.00,tau:5.0,deadTime:0.5},
      4:{K:1.00,tau:20.0,deadTime:2.0}
    };
    const LOG_FIELDS = [
      { id:'logMs', bit:1, label:'Tiempo [ms]' },
      { id:'logInputPct', bit:4, label:'Entrada [%]' },
      { id:'logOutputPct', bit:16, label:'Salida [%]' },
      { id:'logOutputEnabled', bit:32, label:'Salida activa' },
      { id:'logMode', bit:64, label:'Modo' },
      { id:'logPreset', bit:128, label:'Preset' },
      { id:'logK', bit:256, label:'Ganancia (K)' },
      { id:'logTau', bit:512, label:'Tau (τ) [s]' },
      { id:'logDeadTime', bit:1024, label:'Retardo (θ) [s]' },
      { id:'logNoise', bit:2048, label:'Ruido de medición [%]' },
      { id:'logOffset', bit:4096, label:'Offset [%]' },
      { id:'logErrors', bit:8192, label:'Estado / errores' },
      { id:'logCalibrationOutput', bit:16384, label:'Salida en calibración' }
    ];
    const VIEW_META = {
      dashboard:['Operación y visualización','Pantalla principal para operar el equipo y observar variables, tendencia y estado del lazo.'],
      system:['Sistema','Servicios generales, tarjeta SD, audio, WiFi y columnas CSV.'],
      calibration:['Calibración','Puntos RAW para entrada analógica y salida DAC.'],
      about:['Acerca de','Información general del proyecto, versión de interfaz y alcance del sistema.']
    };
    let editing = false;
    let editingLog = false;
    function el(id){ return document.getElementById(id); }
    function setText(id, value){ const x=el(id); if(x) x.textContent = value; }
    function pct(v){ return Number(v || 0).toFixed(1) + ' %'; }
    function clamp(v,lo,hi){ v=Number(v); if(!isFinite(v)) v=lo; return Math.max(lo,Math.min(hi,v)); }
    function notify(msg){ const t=el('toast'); if(!t) return; t.textContent=msg; t.classList.add('show'); clearTimeout(window.__toastTimer); window.__toastTimer=setTimeout(()=>t.classList.remove('show'),3200); }
    function formatUptime(ms){ const total = Math.floor(ms / 1000); const h = Math.floor(total / 3600); const m = Math.floor((total % 3600) / 60); const s = total % 60; return h + 'h ' + m + 'm ' + s + 's'; }
    function pushPoint(inputPct, outputPct){ hist.input.push(Math.max(0, Math.min(100, Number(inputPct || 0)))); hist.output.push(Math.max(0, Math.min(100, Number(outputPct || 0)))); while(hist.input.length > MAX_POINTS) hist.input.shift(); while(hist.output.length > MAX_POINTS) hist.output.shift(); }
    function drawLine(ctx, data, color, plot){ if(data.length < 2) return; ctx.beginPath(); ctx.lineWidth = 2; ctx.strokeStyle = color; data.forEach((v,i)=>{ const x=plot.x+(i/(MAX_POINTS-1))*plot.w; const y=plot.y+plot.h-(v/100)*plot.h; if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y); }); ctx.stroke(); }
    function drawTrend(){ const canvas=el('trend'); if(!canvas) return; const rect=canvas.getBoundingClientRect(); const dpr=window.devicePixelRatio||1; canvas.width=Math.max(1,Math.floor(rect.width*dpr)); canvas.height=Math.max(1,Math.floor(rect.height*dpr)); const ctx=canvas.getContext('2d'); ctx.setTransform(dpr,0,0,dpr,0,0); ctx.clearRect(0,0,rect.width,rect.height); const plot={x:36,y:12,w:Math.max(10,rect.width-48),h:Math.max(10,rect.height-24)}; ctx.strokeStyle='#334155'; ctx.lineWidth=1; ctx.fillStyle='#9ca3af'; ctx.font='11px Arial'; for(let p=0;p<=100;p+=25){ const y=plot.y+plot.h-(p/100)*plot.h; ctx.beginPath(); ctx.moveTo(plot.x,y); ctx.lineTo(plot.x+plot.w,y); ctx.stroke(); ctx.fillText(p+'%',4,y+4); } ctx.strokeStyle='#64748b'; ctx.strokeRect(plot.x,plot.y,plot.w,plot.h); drawLine(ctx,hist.output,getComputedStyle(document.documentElement).getPropertyValue('--accent').trim(),plot); drawLine(ctx,hist.input,getComputedStyle(document.documentElement).getPropertyValue('--input').trim(),plot); }
    function setProcessInputs(presetId,K,tau,deadTime){ el('preset').value=String(presetId); el('procK').value=Number(K||0).toFixed(2); el('procTau').value=Number(tau||0).toFixed(1); el('procDead').value=Number(deadTime||0).toFixed(1); }
    function applyPresetInputs(){ const id=Number(el('preset').value||0); const p=PRESETS[id]||PRESETS[0]; if(id!==0) setProcessInputs(id,p.K,p.tau,p.deadTime); }
    function markCustom(){ el('preset').value='0'; }
    function applyLogInputs(mask){ LOG_FIELDS.forEach(f=>{ const x=el(f.id); if(x) x.checked=(Number(mask||0)&f.bit)!==0; }); }
    function currentLogMask(){ return LOG_FIELDS.reduce((mask,f)=>{ const x=el(f.id); return mask | ((x&&x.checked)?f.bit:0); },0); }
    function describeLogMask(mask){ const cols=LOG_FIELDS.filter(f=>(Number(mask||0)&f.bit)!==0).map(f=>f.label); return cols.length?cols.join(', '):'ninguna'; }
    function toggleSidebar(){ if(window.innerWidth<=1050){ document.body.classList.toggle('sidebar-open'); } else { document.body.classList.toggle('sidebar-collapsed'); } setTimeout(drawTrend,220); }
    function closeSidebar(){ document.body.classList.remove('sidebar-open'); }
    function showView(view){
      document.querySelectorAll('.view').forEach(v=>v.classList.remove('active'));
      document.querySelectorAll('.navBtn').forEach(b=>b.classList.toggle('active',b.dataset.view===view));
      const target=el('view-'+view); if(target) target.classList.add('active');
      const meta=VIEW_META[view]||VIEW_META.dashboard; setText('viewTitle',meta[0]); setText('viewSubtitle',meta[1]);
      if(window.innerWidth<=1050) closeSidebar();
      setTimeout(drawTrend,60);
    }
    function clampManualOutput(){ const x=el('manualOutputPct'); if(!x) return 0; let v=Number(x.value); if(!isFinite(v)) v=0; v=Math.max(0,Math.min(100,v)); x.value=String(Math.round(v)); return v; }
    function updateModeUi(manual){ const m=el('modeManual'), a=el('modeAuto'), hidden=el('manualOutputMode'), out=el('manualOutputPct'); if(m) m.checked=!!manual; if(a) a.checked=!manual; if(hidden) hidden.checked=!!manual; if(out) out.disabled=!manual; }
    function updateCaptureUi(active,lastCsv){ const rec=el('captureRec'), stop=el('captureStop'), dl=el('downloadCsv'); if(rec){ rec.classList.toggle('recording',!!active); rec.title=active?'Captura activa':'Iniciar captura'; rec.disabled=!!active; } if(stop){ stop.classList.toggle('active',!!active); stop.disabled=!active; stop.title=active?'Detener captura':'Captura detenida'; } if(dl){ const hasCsv=!!lastCsv && lastCsv!=='--'; dl.style.pointerEvents=hasCsv?'auto':'none'; dl.style.opacity=hasCsv?'1':'.45'; } }
    function updateCalibrationActual(s){ if(!s.control) return; setText('outputRaw4Actual','Actual: '+s.control.outputRaw4mA); setText('outputRaw20Actual','Actual: '+s.control.outputRaw20mA); setText('inputRawCurrent',s.control.inputRawCurrent); setText('outputRawCurrent',s.control.outputRawCurrent); }
    function setControls(s){
      el('outputEnabled').checked = !!s.process.enabled;
      const manual = (s.process.mode || '').toUpperCase().indexOf('MAN') >= 0;
      updateModeUi(manual);
      el('manualOutputPct').value = Number(s.control ? s.control.manualOutputPct : (s.process.outputPct || 0)).toFixed(0);
      el('manualOutputPct').disabled = !manual;
      setProcessInputs(s.process.preset.id, s.process.model.K, s.process.model.tau, s.process.model.deadTime);
      el('sensorNoise').value = Number(s.process.sensor.noisePct || 0).toFixed(1);
      el('sensorOffset').value = Number(s.process.sensor.offsetPct || 0).toFixed(1);
      el('sensorFreeze').checked = !!(s.control && Number(s.control.sensorFaultMode || 0) !== 0);
      if(s.control){
        const wifiControl = el('wifiEnabled');
        if(wifiControl) wifiControl.checked = !!s.control.wifiEnabled;
        el('audioEnabled').checked = !!s.control.audioEnabled;
        el('loggingEnabled').checked = !!s.control.loggingEnabled;
        el('inputRaw4').value = s.control.inputRaw4mA;
        el('inputRaw20').value = s.control.inputRaw20mA;
        el('outputRaw4').value = s.control.outputRaw4mA;
        el('outputRaw20').value = s.control.outputRaw20mA;
      }
    }
    function setStackLines(id, lines){
      const target=el(id);
      if(!target) return;
      target.innerHTML='';
      lines.forEach(txt=>{
        const line=document.createElement('div');
        line.className='summaryStackLine';
        line.textContent=txt;
        target.appendChild(line);
      });
    }
    function setOutputStateSummary(enabled, modeText){
      const target=el('outputStateInfo');
      if(!target) return;
      target.innerHTML='';
      const stateLine=document.createElement('div');
      stateLine.className='summaryStackLine';
      const stateSpan=document.createElement('span');
      stateSpan.textContent=enabled?'Activa':'Inactiva';
      if(!enabled) stateSpan.className='summaryAttentionInactive';
      stateLine.appendChild(stateSpan);
      target.appendChild(stateLine);
      const modeLine=document.createElement('div');
      modeLine.className='summaryStackLine';
      const modeSpan=document.createElement('span');
      modeSpan.textContent=modeText;
      if(modeText==='Manual') modeSpan.className='summaryAttentionManual';
      modeLine.appendChild(modeSpan);
      target.appendChild(modeLine);
    }
    function renderOperationalFaults(s){
      const target=el('faultInfo');
      if(!target) return;
      const items=[];
      const sensor=s.process&&s.process.sensor?s.process.sensor:{};
      const control=s.control||{};
      const freezeActive=Number(control.sensorFaultMode||0)!==0 || String(sensor.fault||'OK').toUpperCase()==='CONGELADO';
      const noise=Number(sensor.noisePct||0);
      const offset=Number(sensor.offsetPct||0);
      if(freezeActive) items.push('Congelado');
      if(Math.abs(noise)>0.001) items.push('Ruido de medición +/-'+noise.toFixed(1)+' %');
      if(Math.abs(offset)>0.001) items.push('Offset '+(offset>0?'+':'')+offset.toFixed(1)+' %');
      target.className=items.length?'value faultList summaryStack fault':'value faultList summaryStack';
      target.innerHTML='';
      if(!items.length){
        const ok=document.createElement('span');
        ok.className='faultOk';
        ok.textContent='Desactivados';
        target.appendChild(ok);
        return;
      }
      items.forEach(txt=>{
        const line=document.createElement('div');
        line.className='faultItem';
        const value=document.createElement('span');
        value.className='faultItemValue';
        value.textContent=txt;
        line.appendChild(value);
        target.appendChild(line);
      });
    }
    async function loadStatus(){
      try{
        const response=await fetch('/api/status'); const s=await response.json();
        setText('deviceName',s.deviceName); setText('wifiSsid','Red: '+(s.wifi.ssid||'--'));
        setText('outputPct',pct(s.process.outputPct)); setText('inputPct',pct(s.process.inputPct)); const presetNames={0:'Personalizado',1:'Nivel',2:'Caudal',3:'Presión',4:'Temperatura'}; setText('presetInfo',presetNames[Number(s.process.preset.id)]||s.process.preset.name||'--'); const outputMode=(String(s.process.mode||'').toUpperCase().indexOf('MAN')>=0)?'Manual':'Automático'; setOutputStateSummary(!!s.process.enabled,outputMode); setStackLines('modelInfo',['K='+Number(s.process.model.K||0).toFixed(2),'τ='+Number(s.process.model.tau||0).toFixed(1)+' s','Retardo='+Number(s.process.model.deadTime||0).toFixed(1)+' s']);
        renderOperationalFaults(s);
        if(s.control){ updateCalibrationActual(s); const sdTxt=(s.control.sdReady?'OK':'FALLA'); const capTxt=(s.control.loggingEnabled?'SI':'NO'); setText('sdInfo','SD: '+sdTxt); setText('sdCaptureInfo',sdTxt); setText('sdStateMirror',sdTxt); setText('loggingInfo',capTxt); updateCaptureUi(!!s.control.loggingEnabled, s.logging && s.logging.lastCsv); }
        if(s.logging){ const last=s.logging.lastCsv || '--'; setText('lastCsvInfo',last); setText('lastCsvSystem',last); const dl=el('downloadCsv'); if(dl) dl.href='/api/log/download?t='+Date.now(); updateCaptureUi(!!(s.control&&s.control.loggingEnabled), last); }
        if(s.logging){ const mask=Number(s.logging.csvMask||0); if(!editingLog) applyLogInputs(mask); setText('logInfo','Columnas: '+describeLogMask(mask)); }
        if(!editing) setControls(s);
        pushPoint(s.process.inputPct,s.process.outputPct); drawTrend();
      }catch(err){ console.log('Error cargando estado:',err); }
    }
    function userConfigBody(){
      const body=new URLSearchParams();
      body.append('outputEnabled', el('outputEnabled').checked?'1':'0');
      body.append('simulation', el('outputEnabled').checked?'1':'0'); // Parámetro legado para clientes web cacheados.
      body.append('manualMode', el('manualOutputMode').checked?'1':'0');
      body.append('manualPct', String(clampManualOutput()));
      body.append('preset', el('preset').value); body.append('K', clamp(el('procK').value,0,5).toFixed(3)); body.append('tau', clamp(el('procTau').value,0.1,60).toFixed(3)); body.append('deadTime', el('procDead').value);
      body.append('noise', el('sensorNoise').value); body.append('fault', el('sensorFreeze').checked?'1':'0'); body.append('offset', el('sensorOffset').value);
      const wifiControl = el('wifiEnabled'); if(wifiControl) body.append('wifi', wifiControl.checked?'1':'0');
      body.append('audio', el('audioEnabled').checked?'1':'0'); body.append('logging', el('loggingEnabled').checked?'1':'0');
      return body;
    }
    async function saveUserConfig(showNotify=true,source='general',extra=null){
      editing=false;
      const body=userConfigBody();
      body.append('source',source);
      if(extra){ Object.keys(extra).forEach(k=>body.append(k,extra[k])); }
      if(source==='capturaInicio'){
        const mask=currentLogMask();
        if(mask===0){
          const x=el('loggingEnabled');
          if(x) x.checked=false;
          notify('Seleccioná al menos una columna para el CSV.');
          return;
        }
        body.append('csvMask',String(mask));
        editingLog=false;
      }
      const response=await fetch('/api/user/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body.toString()});
      const msg=await response.text();
      if(showNotify) notify(msg);
      loadStatus();
    }
    async function setCaptureEnabled(enabled){ const x=el('loggingEnabled'); if(x) x.checked=!!enabled; await saveUserConfig(true,enabled?'capturaInicio':'capturaDetencion'); }
    async function saveLogConfig(){ const mask=currentLogMask(); if(mask===0){notify('Seleccioná al menos una columna para el CSV.'); return;} editingLog=false; const body=new URLSearchParams(); body.append('mask',String(mask)); const response=await fetch('/api/log/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body.toString()}); notify(await response.text()); loadStatus(); }
    function calibrationRawValues(){ const raw4=Math.round(Number(el('outputRaw4').value)); const raw20=Math.round(Number(el('outputRaw20').value)); if(!Number.isFinite(raw4)||!Number.isFinite(raw20)||raw4<0||raw20>4095||raw20<=raw4||(raw20-raw4)<100){ notify('Calibración de salida inválida. Revisá RAW 4 mA y RAW 20 mA.'); return null; } return {out4:String(raw4),out20:String(raw20)}; }
    async function saveOutputCalibration(){ const values=calibrationRawValues(); if(!values) return; await saveUserConfig(true,'calibracionSalida',values); }
    async function triggerAuthorDetail(ev){ if(ev) ev.preventDefault(); const body=new URLSearchParams(); body.append('action','special'); try{ await fetch('/api/user/action',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body.toString()}); }catch(err){ console.log('Error enviando acción:',err); } }
    const ACTION_CONFIRM={
      wifiReset:'¿Seguro que querés borrar las credenciales WiFi guardadas?',
      saveData:'¿Guardar una captura manual de datos?',
      calInput4:'¿Tomar el valor actual de entrada como referencia de 4 mA?',
      calInput20:'¿Tomar el valor actual de entrada como referencia de 20 mA?',
      testOut4:'¿Probar la salida en 4 mA con los RAW ingresados?',
      testOut20:'¿Probar la salida en 20 mA con los RAW ingresados?'
    };
    async function sendAction(action,extra){ const question=ACTION_CONFIRM[action]; if(question&&!confirm(question)) return; const body=new URLSearchParams(); body.append('action',action); if(extra){ Object.keys(extra).forEach(k=>body.append(k,extra[k])); } const response=await fetch('/api/user/action',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body.toString()}); notify(await response.text()); loadStatus(); }
    async function sendCalibrationTest(action){ const values=calibrationRawValues(); if(!values) return; await sendAction(action,values); }
    async function saveWiFi(){ const body=new URLSearchParams(); body.append('ssid',el('ssid').value); body.append('pass',el('pass').value); const response=await fetch('/api/wifi/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body.toString()}); notify(await response.text()); }
    async function restartDevice(){ if(!confirm('¿Seguro que querés reiniciar el equipo?')) return; await fetch('/api/restart',{method:'POST'}); notify('Equipo reiniciando...'); }
    document.querySelectorAll('.navBtn').forEach(btn=>btn.addEventListener('click',()=>showView(btn.dataset.view)));
    async function saveModelSilently(){ editing=false; await saveUserConfig(false); }
    el('preset').addEventListener('change',async()=>{editing=true;applyPresetInputs();await saveModelSilently();});
    ['procK','procTau','procDead'].forEach(id=>{ const x=el(id); x.addEventListener('focus',()=>{editing=true;}); x.addEventListener('input',()=>{editing=true;markCustom();}); x.addEventListener('change',async()=>{await saveModelSilently();}); x.addEventListener('keydown',async(ev)=>{if(ev.key==='Enter'){ev.preventDefault();x.blur();await saveModelSilently();}}); });
    ['sensorNoise','sensorOffset'].forEach(id=>{ const x=el(id); if(x){ x.addEventListener('focus',()=>{editing=true;}); x.addEventListener('input',()=>{editing=true;}); x.addEventListener('change',async()=>{await saveModelSilently();}); x.addEventListener('keydown',async(ev)=>{if(ev.key==='Enter'){ev.preventDefault();x.blur();await saveModelSilently();}}); }});
    el('sensorFreeze').addEventListener('change',async()=>{await saveModelSilently();});
    ['manualOutputMode','loggingEnabled','outputRaw4','outputRaw20'].forEach(id=>{ const x=el(id); if(x){ x.addEventListener('focus',()=>{editing=true;}); x.addEventListener('input',()=>{editing=true;}); x.addEventListener('change',()=>{editing=true;}); }});
    el('audioEnabled').addEventListener('change',async()=>{ editing=false; await saveUserConfig(true,'audio'); });
    el('outputEnabled').addEventListener('change',async()=>{ editing=false; await saveUserConfig(true,'salida'); });
    el('modeManual').addEventListener('change',async()=>{ editing=false; updateModeUi(!!el('modeManual').checked); await saveUserConfig(true,'modo'); });
    el('modeAuto').addEventListener('change',async()=>{ editing=false; updateModeUi(!el('modeAuto').checked); await saveUserConfig(true,'modo'); });
    el('manualOutputPct').addEventListener('focus',()=>{editing=true;});
    el('manualOutputPct').addEventListener('input',()=>{editing=true; clampManualOutput();});
    el('manualOutputPct').addEventListener('change',async()=>{editing=false; clampManualOutput(); await saveUserConfig(true,'salidaManual');});
    LOG_FIELDS.forEach(f=>{ const x=el(f.id); if(x) x.addEventListener('change',()=>{editingLog=true;}); });
    window.addEventListener('resize',drawTrend); loadStatus(); setInterval(loadStatus,1000);
  </script>
</body>
</html>
)rawliteral";

InterfaceWiFiClass::InterfaceWiFiClass()
  : _server(80) {}

bool InterfaceWiFiClass::begin(const char* hostname,
                               const char* apSsid,
                               const char* apPass,
                               uint32_t stationTimeoutMs)
{
  if (_active) {
    return isStationConnected();
  }

  _hostname = hostname ? hostname : "simulador-industrial";
  _apSsid   = apSsid   ? apSsid   : "Simulador420_Wifi";
  _apPass   = apPass   ? apPass   : "";
  _stationTimeoutMs = stationTimeoutMs;

  _prefs.begin("ifwifi", false);
  _savedSsid = _prefs.getString("ssid", "");
  _savedPass = _prefs.getString("pass", "");
  _csvLogMask = sanitizeCsvLogMask((uint16_t)_prefs.getUInt("csvMask", IFW_CSV_DEFAULT_MASK));

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  if (_savedSsid.length() > 0) {
    startStationAttempt();
  } else {
    startAccessPoint();
  }

  if (!_routesConfigured) {
    setupRoutes();
    _routesConfigured = true;
  }

  if (!_serverStarted) {
    _server.begin();
    _serverStarted = true;
  }

  _active = true;

  Serial.println();
  Serial.println("InterfaceWiFi lista.");
  Serial.print("Modo WiFi: ");
  Serial.println(getModeString());
  Serial.print("SSID: ");
  Serial.println(getSSIDString());
  Serial.print("IP: ");
  Serial.println(getIpString());

  return isStationConnected();
}

void InterfaceWiFiClass::handle()
{
  if (!_active) return;

  if (_serverStarted) {
    _server.handleClient();
  }

  updateConnectionState();
  updatePendingRestart();
}

void InterfaceWiFiClass::end()
{
  if (_serverStarted) {
    _server.stop();
    _serverStarted = false;
  }

  WiFi.disconnect(true, true);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);

  _connectionState = CONN_IDLE;
  _active = false;
  _restartPending = false;
}

void InterfaceWiFiClass::clearCredentials()
{
  Preferences p;
  p.begin("ifwifi", false);
  p.remove("ssid");
  p.remove("pass");
  p.end();

  _savedSsid = "";
  _savedPass = "";

  Serial.println("Credenciales WiFi borradas.");
}

bool InterfaceWiFiClass::isActive() const
{
  return _active;
}

bool InterfaceWiFiClass::isConnecting() const
{
  return _active && _connectionState == CONN_CONNECTING;
}

bool InterfaceWiFiClass::isStationConnected() const
{
  return _active && WiFi.status() == WL_CONNECTED;
}

bool InterfaceWiFiClass::isAccessPointMode() const
{
  if (!_active) return false;
  const wifi_mode_t mode = WiFi.getMode();
  return mode == WIFI_AP || mode == WIFI_AP_STA || _connectionState == CONN_AP;
}

void InterfaceWiFiClass::setDeviceName(const String& name)
{
  _deviceName = name;
}

void InterfaceWiFiClass::setProjectState(const String& state)
{
  _projectState = state;
}


void InterfaceWiFiClass::setProcessPercent(bool enabled,
                                           const String& mode,
                                           float outputPct,
                                           float inputPct,
                                           const String& note)
{
  _channel.enabled = enabled;
  _channel.mode = mode;
  _channel.outputPct = constrain(outputPct, 0.0f, 100.0f);
  _channel.inputPct = constrain(inputPct, 0.0f, 100.0f);
  _channel.note = note;
}

void InterfaceWiFiClass::setProcessDiagnostics(ProcessPreset preset,
                                               float K,
                                               float tau,
                                               float deadTime,
                                               float noisePct,
                                               const String& sensorFault,
                                               float sensorOffsetPct,
                                               bool faultActive)
{
  _channel.processPreset = sanitizeProcessPreset(static_cast<uint8_t>(preset));
  _channel.processK = K;
  _channel.processTau = tau;
  _channel.processDeadTime = deadTime;
  _channel.sensorNoisePct = constrain(noisePct, 0.0f, 100.0f);
  _channel.sensorFault = sensorFault;
  _channel.sensorOffsetPct = sensorOffsetPct;
  _channel.faultActive = faultActive;
}

void InterfaceWiFiClass::setUserConfigSnapshot(bool outputEnabled,
                                                bool manualOutputMode,
                                                float manualOutputPct,
                                                bool wifiEnabled,
                                                bool audioEnabled,
                                                bool sdReady,
                                                bool loggingEnabled,
                                                uint16_t inputRaw4mA,
                                                uint16_t inputRaw20mA,
                                                uint16_t inputRawCurrent,
                                                uint16_t outputRawCurrent,
                                                uint16_t outputRaw4mA,
                                                uint16_t outputRaw20mA,
                                                uint8_t sensorFaultMode)
{
  _controlSnapshot.outputEnabled = outputEnabled;
  _controlSnapshot.manualOutputMode = manualOutputMode;
  _controlSnapshot.manualOutputPct = constrain(manualOutputPct, 0.0f, 100.0f);
  _controlSnapshot.preset = _channel.processPreset;
  _controlSnapshot.K = _channel.processK;
  _controlSnapshot.tau = _channel.processTau;
  _controlSnapshot.deadTime = _channel.processDeadTime;
  _controlSnapshot.sensorNoisePct = _channel.sensorNoisePct;
  _controlSnapshot.sensorFaultMode = sensorFaultMode;
  _controlSnapshot.sensorOffsetPct = _channel.sensorOffsetPct;
  _controlSnapshot.wifiEnabled = wifiEnabled;
  _controlSnapshot.audioEnabled = audioEnabled;
  _controlSnapshot.loggingEnabled = loggingEnabled;
  _controlSnapshot.outputRaw4mA = outputRaw4mA;
  _controlSnapshot.outputRaw20mA = outputRaw20mA;
  _sdReady = sdReady;
  _inputRaw4mA = inputRaw4mA;
  _inputRaw20mA = inputRaw20mA;
  _inputRawCurrent = inputRawCurrent;
  _outputRawCurrent = outputRawCurrent;
}

bool InterfaceWiFiClass::consumeProcessConfigRequest(InterfaceWiFiProcessConfig& out)
{
  if (!_processConfigPending) return false;

  out = _pendingProcessConfig;
  _processConfigPending = false;
  return true;
}

bool InterfaceWiFiClass::consumeLogConfigRequest(InterfaceWiFiLogConfig& out)
{
  if (!_logConfigPending) return false;

  out = _pendingLogConfig;
  _logConfigPending = false;
  return true;
}

bool InterfaceWiFiClass::consumeUserConfigRequest(InterfaceWiFiUserConfig& out)
{
  if (!_userConfigPending) return false;

  out = _pendingUserConfig;
  _userConfigPending = false;
  return true;
}

bool InterfaceWiFiClass::consumeUserActionRequest(InterfaceWiFiUserActionRequest& out)
{
  if (!_userActionPending) return false;

  out = _pendingUserAction;
  _pendingUserAction.action = IFW_ACTION_NONE;
  _userActionPending = false;
  return true;
}

uint16_t InterfaceWiFiClass::getCsvLogMask() const
{
  return _csvLogMask;
}

void InterfaceWiFiClass::setCsvLogMask(uint16_t mask)
{
  _csvLogMask = sanitizeCsvLogMask(mask);
}

void InterfaceWiFiClass::setLastCsvPath(const char* path)
{
  if (!path || !path[0]) {
    _lastCsvPath = "";
    return;
  }

  _lastCsvPath = path;
  if (!_lastCsvPath.startsWith("/")) {
    _lastCsvPath = "/" + _lastCsvPath;
  }
}

void InterfaceWiFiClass::startStationAttempt()
{
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname(_hostname.c_str());
  WiFi.begin(_savedSsid.c_str(), _savedPass.c_str());

  _connectionState = CONN_CONNECTING;
  _connectStartMs = millis();

  Serial.println("Intentando conectar a WiFi guardado...");
  Serial.print("SSID: ");
  Serial.println(_savedSsid);
}

void InterfaceWiFiClass::startAccessPoint()
{
  WiFi.mode(WIFI_AP);

  bool apStarted = false;
  if (_apPass.length() >= 8) {
    apStarted = WiFi.softAP(_apSsid.c_str(), _apPass.c_str());
  } else {
    // WPA/WPA2 no permite claves menores a 8 caracteres. Si se configura "1234",
    // el AP se levanta abierto para no fallar silenciosamente.
    apStarted = WiFi.softAP(_apSsid.c_str());
  }

  _connectionState = CONN_AP;

  Serial.println("Levantando Access Point...");
  Serial.print("AP SSID: ");
  Serial.println(_apSsid);
  Serial.print("AP protegido: ");
  Serial.println((_apPass.length() >= 8 && apStarted) ? "SI" : "NO");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void InterfaceWiFiClass::saveCredentials(const String& ssid, const String& pass)
{
  _prefs.putString("ssid", ssid);
  _prefs.putString("pass", pass);
  _savedSsid = ssid;
  _savedPass = pass;

  Serial.println("Credenciales WiFi guardadas.");
}

void InterfaceWiFiClass::updateConnectionState()
{
  if (_connectionState == CONN_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      _connectionState = CONN_CONNECTED;
      Serial.println("WiFi conectado.");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      return;
    }

    const uint32_t now = millis();
    if ((uint32_t)(now - _connectStartMs) >= _stationTimeoutMs) {
      Serial.println("No se pudo conectar al WiFi guardado.");
      startAccessPoint();
    }
  } else if (_connectionState == CONN_CONNECTED) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Se perdio la conexion WiFi. Volviendo a AP.");
      startAccessPoint();
    }
  }
}

void InterfaceWiFiClass::updatePendingRestart()
{
  if (_restartPending && (int32_t)(millis() - _restartAtMs) >= 0) {
    ESP.restart();
  }
}

void InterfaceWiFiClass::setupRoutes()
{
  _server.on("/", HTTP_GET, [this]() { handleRoot(); });
  _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  _server.on("/api/wifi/save", HTTP_POST, [this]() { handleSaveWiFi(); });
  _server.on("/api/process/config", HTTP_POST, [this]() { handleSaveProcessConfig(); });
  _server.on("/api/log/config", HTTP_POST, [this]() { handleSaveLogConfig(); });
  _server.on("/api/log/download", HTTP_GET, [this]() { handleDownloadLastCsv(); });
  _server.on("/api/user/config", HTTP_POST, [this]() { handleSaveUserConfig(); });
  _server.on("/api/user/action", HTTP_POST, [this]() { handleUserAction(); });
  _server.on("/api/restart", HTTP_POST, [this]() { handleRestart(); });
  _server.onNotFound([this]() { handleNotFound(); });
}

void InterfaceWiFiClass::handleRoot()
{
  _server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void InterfaceWiFiClass::handleStatus()
{
  _server.send(200, "application/json; charset=utf-8", buildStatusJson());
}

void InterfaceWiFiClass::handleSaveWiFi()
{
  if (!_server.hasArg("ssid")) {
    _server.send(400, "text/plain; charset=utf-8", "Falta parámetro ssid.");
    return;
  }

  const String ssid = _server.arg("ssid");
  const String pass = _server.arg("pass");

  if (ssid.length() == 0) {
    _server.send(400, "text/plain; charset=utf-8", "El SSID no puede estar vacío.");
    return;
  }

  saveCredentials(ssid, pass);
  _server.send(200, "text/plain; charset=utf-8", "Credenciales WiFi guardadas. Reiniciá el equipo o apagá y encendé WiFi desde el menú.");
}

void InterfaceWiFiClass::handleSaveProcessConfig()
{
  InterfaceWiFiProcessConfig cfg;
  cfg.preset = sanitizeProcessPreset((uint8_t)_server.arg("preset").toInt());

  if (processPresetIsAutomatic(cfg.preset)) {
    const ProcessPresetDefinition& def = processPresetDefinition(cfg.preset);
    cfg.K = def.K;
    cfg.tau = def.tau_s;
    cfg.deadTime = def.deadTime_s;
  } else {
    cfg.K = _server.hasArg("K") ? _server.arg("K").toFloat() : _channel.processK;
    cfg.tau = _server.hasArg("tau") ? _server.arg("tau").toFloat() : _channel.processTau;
    cfg.deadTime = _server.hasArg("deadTime") ? _server.arg("deadTime").toFloat() : _channel.processDeadTime;

    cfg.K = constrain(cfg.K, PROCESS_K_MIN, PROCESS_K_MAX);
    cfg.tau = constrain(cfg.tau, PROCESS_TAU_MIN_S, PROCESS_TAU_MAX_S);
    cfg.deadTime = constrain(cfg.deadTime, 0.0f, 30.0f);
  }

  _channel.processPreset = cfg.preset;
  _channel.processK = cfg.K;
  _channel.processTau = cfg.tau;
  _channel.processDeadTime = cfg.deadTime;

  _pendingProcessConfig = cfg;
  _processConfigPending = true;

  String msg = "Modelo aplicado: ";
  msg += processPresetName(cfg.preset);
  _server.send(200, "text/plain; charset=utf-8", msg);
}

void InterfaceWiFiClass::handleSaveLogConfig()
{
  if (!_server.hasArg("mask")) {
    _server.send(400, "text/plain; charset=utf-8", "Falta parámetro mask.");
    return;
  }

  const uint16_t mask = sanitizeCsvLogMask((uint16_t)_server.arg("mask").toInt());

  _csvLogMask = mask;
  _prefs.putUInt("csvMask", _csvLogMask);

  _pendingLogConfig.csvMask = _csvLogMask;
  _logConfigPending = true;

  _server.send(200, "text/plain; charset=utf-8", "Configuración de columnas CSV guardada.");
}

void InterfaceWiFiClass::handleDownloadLastCsv()
{
  if (_lastCsvPath.length() == 0) {
    _server.send(404, "text/plain; charset=utf-8", "No hay CSV generado todavía.");
    return;
  }

  String path = _lastCsvPath;
  if (!path.startsWith("/")) {
    path = "/" + path;
  }

  if (!SD.exists(path)) {
    _server.send(404, "text/plain; charset=utf-8", "No se encontró el último CSV en la SD.");
    return;
  }

  File f = SD.open(path, FILE_READ);
  if (!f) {
    _server.send(500, "text/plain; charset=utf-8", "No se pudo abrir el CSV.");
    return;
  }

  String fileName = path;
  const int slash = fileName.lastIndexOf('/');
  if (slash >= 0) {
    fileName = fileName.substring(slash + 1);
  }

  _server.sendHeader("Content-Disposition", "attachment; filename=\"" + fileName + "\"");
  _server.streamFile(f, "text/csv");
  f.close();
}

void InterfaceWiFiClass::handleSaveUserConfig()
{
  InterfaceWiFiUserConfig cfg;

  cfg.outputEnabled = argBool("outputEnabled", argBool("simulation", _controlSnapshot.outputEnabled));
  cfg.manualOutputMode = argBool("manualMode", _controlSnapshot.manualOutputMode);
  cfg.manualOutputPct = argFloat("manualPct", _controlSnapshot.manualOutputPct, 0.0f, 100.0f);

  cfg.preset = sanitizeProcessPreset((uint8_t)_server.arg("preset").toInt());
  if (processPresetIsAutomatic(cfg.preset)) {
    const ProcessPresetDefinition& def = processPresetDefinition(cfg.preset);
    cfg.K = def.K;
    cfg.tau = def.tau_s;
    cfg.deadTime = def.deadTime_s;
  } else {
    cfg.K = argFloat("K", _controlSnapshot.K, PROCESS_K_MIN, PROCESS_K_MAX);
    cfg.tau = argFloat("tau", _controlSnapshot.tau, PROCESS_TAU_MIN_S, PROCESS_TAU_MAX_S);
    cfg.deadTime = argFloat("deadTime", _controlSnapshot.deadTime, 0.0f, 30.0f);
  }

  cfg.sensorNoisePct = argFloat("noise", _controlSnapshot.sensorNoisePct, 0.0f, 10.0f);
  cfg.sensorFaultMode = (uint8_t)constrain(_server.arg("fault").toInt(), 0, 1);
  cfg.sensorOffsetPct = argFloat("offset", _controlSnapshot.sensorOffsetPct, -25.0f, 25.0f);

  cfg.wifiEnabled = argBool("wifi", _controlSnapshot.wifiEnabled);
  cfg.audioEnabled = argBool("audio", _controlSnapshot.audioEnabled);
  cfg.loggingEnabled = argBool("logging", _controlSnapshot.loggingEnabled);

  const String source = _server.hasArg("source") ? _server.arg("source") : "general";
  cfg.outputRaw4mA = _controlSnapshot.outputRaw4mA;
  cfg.outputRaw20mA = _controlSnapshot.outputRaw20mA;
  cfg.outputCalibrationUpdate = false;

  if (source == "calibracionSalida") {
    if (!_server.hasArg("out4") || !_server.hasArg("out20")) {
      _server.send(400, "text/plain; charset=utf-8", "Faltan los valores RAW de calibración.");
      return;
    }

    const uint16_t raw4 = argU16("out4", _controlSnapshot.outputRaw4mA, 0, 4095);
    const uint16_t raw20 = argU16("out20", _controlSnapshot.outputRaw20mA, 0, 4095);

    if (raw20 <= raw4 || (raw20 - raw4) < 100U) {
      _server.send(400, "text/plain; charset=utf-8", "Calibración de salida inválida. Revisá RAW 4 mA y RAW 20 mA.");
      return;
    }

    cfg.outputRaw4mA = raw4;
    cfg.outputRaw20mA = raw20;
    cfg.outputCalibrationUpdate = true;
  }

  if (_server.hasArg("csvMask")) {
    _csvLogMask = sanitizeCsvLogMask((uint16_t)_server.arg("csvMask").toInt());
    _prefs.putUInt("csvMask", _csvLogMask);
  }

  _pendingUserConfig = cfg;
  _userConfigPending = true;

  _controlSnapshot = cfg;
  _channel.enabled = cfg.outputEnabled;
  _channel.mode = cfg.manualOutputMode ? "MANUAL" : "AUTO";
  _channel.processPreset = cfg.preset;
  _channel.processK = cfg.K;
  _channel.processTau = cfg.tau;
  _channel.processDeadTime = cfg.deadTime;
  _channel.sensorNoisePct = cfg.sensorNoisePct;
  _channel.sensorOffsetPct = cfg.sensorOffsetPct;
  _channel.sensorFault = cfg.sensorFaultMode ? "Congelado" : "OK";
  _channel.faultActive = cfg.sensorFaultMode != 0 || cfg.sensorNoisePct > 0.0f || fabsf(cfg.sensorOffsetPct) > 0.001f;

  String msg;
  if (source == "calibracionSalida") {
    msg = "Calibración de salida enviada al equipo.";
  } else if (source == "salida") {
    msg = cfg.outputEnabled ? "Salida activada." : "Salida desactivada.";
  } else if (source == "modo") {
    msg = cfg.manualOutputMode ? "Modo manual activado." : "Modo automático activado.";
  } else if (source == "salidaManual") {
    msg = "Salida manual actualizada.";
  } else if (source == "capturaInicio") {
    msg = "Captura CSV iniciada.";
  } else if (source == "capturaDetencion") {
    msg = "Captura CSV detenida.";
  } else if (source == "audio") {
    msg = cfg.audioEnabled ? "Audio activado." : "Audio desactivado.";
  } else {
    msg = "Configuración guardada.";
  }

  _server.send(200, "text/plain; charset=utf-8", msg);
}

void InterfaceWiFiClass::handleUserAction()
{
  if (!_server.hasArg("action")) {
    _server.send(400, "text/plain; charset=utf-8", "Falta parámetro action.");
    return;
  }

  const String action = _server.arg("action");
  InterfaceWiFiUserActionRequest req;

  if (action == "wifiReset") {
    req.action = IFW_ACTION_WIFI_RESET;
  } else if (action == "sdDetect") {
    req.action = IFW_ACTION_SD_DETECT;
  } else if (action == "saveData") {
    req.action = IFW_ACTION_SAVE_DATA;
  } else if (action == "calInput4") {
    req.action = IFW_ACTION_CAL_INPUT_4;
  } else if (action == "calInput20") {
    req.action = IFW_ACTION_CAL_INPUT_20;
  } else if (action == "testOut4") {
    req.action = IFW_ACTION_TEST_OUTPUT_4;
    req.outputRaw4mA = argU16("out4", _controlSnapshot.outputRaw4mA, 0, 4095);
    req.outputRaw20mA = argU16("out20", _controlSnapshot.outputRaw20mA, 0, 4095);
  } else if (action == "testOut20") {
    req.action = IFW_ACTION_TEST_OUTPUT_20;
    req.outputRaw4mA = argU16("out4", _controlSnapshot.outputRaw4mA, 0, 4095);
    req.outputRaw20mA = argU16("out20", _controlSnapshot.outputRaw20mA, 0, 4095);
  } else if (action == "special") {
    req.action = IFW_ACTION_SPECIAL;
  } else {
    _server.send(400, "text/plain; charset=utf-8", "Acción desconocida.");
    return;
  }

  _pendingUserAction = req;
  _userActionPending = true;

  String msg;
  if (action == "wifiReset") {
    msg = "Borrado de credenciales WiFi solicitado.";
  } else if (action == "sdDetect") {
    msg = "Detección de tarjeta SD solicitada.";
  } else if (action == "saveData") {
    msg = "Guardado manual de datos solicitado.";
  } else if (action == "calInput4") {
    msg = "Calibración de entrada 4 mA solicitada.";
  } else if (action == "calInput20") {
    msg = "Calibración de entrada 20 mA solicitada.";
  } else if (action == "testOut4") {
    msg = "Prueba de salida 4 mA solicitada.";
  } else if (action == "testOut20") {
    msg = "Prueba de salida 20 mA solicitada.";
  } else if (action == "special") {
    msg = "OK";
  } else {
    msg = "Acción recibida.";
  }

  _server.send(200, "text/plain; charset=utf-8", msg);
}

void InterfaceWiFiClass::handleRestart()
{
  _server.send(200, "text/plain; charset=utf-8", "Equipo reiniciando...");
  _restartPending = true;
  _restartAtMs = millis() + 300;
}

void InterfaceWiFiClass::handleNotFound()
{
  _server.send(404, "text/plain; charset=utf-8", "Recurso no encontrado.");
}

String InterfaceWiFiClass::jsonEscape(const String& input) const
{
  String out;
  out.reserve(input.length() + 8);

  for (size_t i = 0; i < input.length(); i++) {
    const char c = input[i];
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }

  return out;
}

String InterfaceWiFiClass::buildStatusJson() const
{
  String s;
  s.reserve(1600);

  s += "{";
  s += "\"deviceName\":\"" + jsonEscape(_deviceName) + "\",";
  s += "\"projectState\":\"" + jsonEscape(_projectState) + "\",";

  s += "\"wifi\":{";
  s += "\"mode\":\"" + jsonEscape(getModeString()) + "\",";
  s += "\"ssid\":\"" + jsonEscape(getSSIDString()) + "\",";
  s += "\"ip\":\"" + jsonEscape(getIpString()) + "\",";
  s += "\"rssi\":" + String(getRSSI());
  s += "},";

  s += "\"uptimeMs\":" + String(millis()) + ",";

  s += "\"logging\":{";
  s += "\"csvMask\":" + String(_csvLogMask) + ",";
  s += "\"lastCsv\":\"" + jsonEscape(_lastCsvPath) + "\"";
  s += "},";

  s += "\"control\":{";
  s += "\"manualOutputPct\":" + String(_controlSnapshot.manualOutputPct, 1) + ",";
  s += "\"wifiEnabled\":" + String(_controlSnapshot.wifiEnabled ? "true" : "false") + ",";
  s += "\"audioEnabled\":" + String(_controlSnapshot.audioEnabled ? "true" : "false") + ",";
  s += "\"sdReady\":" + String(_sdReady ? "true" : "false") + ",";
  s += "\"loggingEnabled\":" + String(_controlSnapshot.loggingEnabled ? "true" : "false") + ",";
  s += "\"sensorFaultMode\":" + String(_controlSnapshot.sensorFaultMode) + ",";
  s += "\"inputRaw4mA\":" + String(_inputRaw4mA) + ",";
  s += "\"inputRaw20mA\":" + String(_inputRaw20mA) + ",";
  s += "\"inputRawCurrent\":" + String(_inputRawCurrent) + ",";
  s += "\"outputRawCurrent\":" + String(_outputRawCurrent) + ",";
  s += "\"outputRaw4mA\":" + String(_controlSnapshot.outputRaw4mA) + ",";
  s += "\"outputRaw20mA\":" + String(_controlSnapshot.outputRaw20mA);
  s += "},";

  s += "\"process\":{";
  s += "\"enabled\":" + String(_channel.enabled ? "true" : "false") + ",";
  s += "\"mode\":\"" + jsonEscape(_channel.mode) + "\",";
  s += "\"outputPct\":" + String(_channel.outputPct, 1) + ",";
  s += "\"inputPct\":" + String(_channel.inputPct, 1) + ",";
  s += "\"preset\":{";
  s += "\"id\":" + String((uint8_t)_channel.processPreset) + ",";
  s += "\"name\":\"" + jsonEscape(processPresetName(_channel.processPreset)) + "\"";
  s += "},";
  s += "\"model\":{";
  s += "\"K\":" + String(_channel.processK, 3) + ",";
  s += "\"tau\":" + String(_channel.processTau, 3) + ",";
  s += "\"deadTime\":" + String(_channel.processDeadTime, 3);
  s += "},";
  s += "\"sensor\":{";
  s += "\"noisePct\":" + String(_channel.sensorNoisePct, 2) + ",";
  s += "\"offsetPct\":" + String(_channel.sensorOffsetPct, 2) + ",";
  s += "\"fault\":\"" + jsonEscape(_channel.sensorFault) + "\",";
  s += "\"active\":" + String(_channel.faultActive ? "true" : "false");
  s += "},";
  s += "\"note\":\"" + jsonEscape(_channel.note) + "\"";
  s += "}";

  s += "}";
  return s;
}

uint16_t InterfaceWiFiClass::sanitizeCsvLogMask(uint16_t mask) const
{
  mask &= IFW_CSV_ALL_MASK;
  if (mask == 0) {
    return IFW_CSV_DEFAULT_MASK;
  }
  return mask;
}

bool InterfaceWiFiClass::argBool(const char* name, bool fallback)
{
  if (!_server.hasArg(name)) return fallback;
  const String v = _server.arg(name);
  return v == "1" || v == "true" || v == "on" || v == "ON";
}

float InterfaceWiFiClass::argFloat(const char* name, float fallback, float lo, float hi)
{
  if (!_server.hasArg(name)) return fallback;
  return constrain(_server.arg(name).toFloat(), lo, hi);
}

uint16_t InterfaceWiFiClass::argU16(const char* name, uint16_t fallback, uint16_t lo, uint16_t hi)
{
  if (!_server.hasArg(name)) return fallback;
  int32_t value = _server.arg(name).toInt();
  if (value < (int32_t)lo) value = lo;
  if (value > (int32_t)hi) value = hi;
  return (uint16_t)value;
}

String InterfaceWiFiClass::getModeString() const
{
  if (!_active) return "OFF";
  if (WiFi.status() == WL_CONNECTED) return "STA";
  const wifi_mode_t mode = WiFi.getMode();
  if (mode == WIFI_AP || mode == WIFI_AP_STA) return "AP";
  if (_connectionState == CONN_CONNECTING) return "CONNECTING";
  return "OFF";
}

String InterfaceWiFiClass::getIpString() const
{
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  const wifi_mode_t mode = WiFi.getMode();
  if (mode == WIFI_AP || mode == WIFI_AP_STA) return WiFi.softAPIP().toString();
  return "0.0.0.0";
}

String InterfaceWiFiClass::getSSIDString() const
{
  if (WiFi.status() == WL_CONNECTED) return WiFi.SSID();
  const wifi_mode_t mode = WiFi.getMode();
  if (mode == WIFI_AP || mode == WIFI_AP_STA) return WiFi.softAPSSID();
  if (_connectionState == CONN_CONNECTING) return _savedSsid;
  return "--";
}

int32_t InterfaceWiFiClass::getRSSI() const
{
  if (WiFi.status() == WL_CONNECTED) return WiFi.RSSI();
  return 0;
}
