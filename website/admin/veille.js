/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Styves Goupil
 * The veille (veille.html): the mentions the agent found, from /api/veille/mentions (server/veille.py), gone through by
 * the buttons, the keys or the thumb. A change shows at once and is sent after (a failure puts it back); what is
 * done with leaves the list, UNDO brings it back where it was. A link opened marks it seen and leaves it there, to answer
 * it once back. Everything here comes from the web: shown as text, its links https from the service. The platforms'
 * marks: Simple Icons (CC0). */
"use strict";
(() => {
  const $ = (s, r = document) => r.querySelector(s);
  const NS = "http://www.w3.org/2000/svg";
  const h = (tag, props = {}, ...kids) => {
    const el = document.createElement(tag);
    for (const [k, v] of Object.entries(props)) {
      if (v === null || v === undefined || v === false) continue;
      if (k === "class") el.className = v;
      else if (k === "text") el.textContent = v;
      else if (k.startsWith("on")) el.addEventListener(k.slice(2), v);
      else el.setAttribute(k, v === true ? "" : v);
    }
    for (const c of kids.flat()) if (c !== null && c !== undefined && c !== false) el.append(c);
    return el;
  };
  const MARKS = {
    youtube: "M23.498 6.186a3.016 3.016 0 0 0-2.122-2.136C19.505 3.545 12 3.545 12 3.545s-7.505 0-9.377.505A3.017 3.017 0 0 0 .502 6.186C0 8.07 0 12 0 12s0 3.93.502 5.814a3.016 3.016 0 0 0 2.122 2.136c1.871.505 9.376.505 9.376.505s7.505 0 9.377-.505a3.015 3.015 0 0 0 2.122-2.136C24 15.93 24 12 24 12s0-3.93-.502-5.814zM9.545 15.568V8.432L15.818 12l-6.273 3.568z",
    reddit: "M12 0C5.373 0 0 5.373 0 12c0 3.314 1.343 6.314 3.515 8.485l-2.286 2.286C.775 23.225 1.097 24 1.738 24H12c6.627 0 12-5.373 12-12S18.627 0 12 0Zm4.388 3.199c1.104 0 1.999.895 1.999 1.999 0 1.105-.895 2-1.999 2-.946 0-1.739-.657-1.947-1.539v.002c-1.147.162-2.032 1.15-2.032 2.341v.007c1.776.067 3.4.567 4.686 1.363.473-.363 1.064-.58 1.707-.58 1.547 0 2.802 1.254 2.802 2.802 0 1.117-.655 2.081-1.601 2.531-.088 3.256-3.637 5.876-7.997 5.876-4.361 0-7.905-2.617-7.998-5.87-.954-.447-1.614-1.415-1.614-2.538 0-1.548 1.255-2.802 2.803-2.802.645 0 1.239.218 1.712.585 1.275-.79 2.881-1.291 4.64-1.365v-.01c0-1.663 1.263-3.034 2.88-3.207.188-.911.993-1.595 1.959-1.595Zm-8.085 8.376c-.784 0-1.459.78-1.506 1.797-.047 1.016.64 1.429 1.426 1.429.786 0 1.371-.369 1.418-1.385.047-1.017-.553-1.841-1.338-1.841Zm7.406 0c-.786 0-1.385.824-1.338 1.841.047 1.017.634 1.385 1.418 1.385.785 0 1.473-.413 1.426-1.429-.046-1.017-.721-1.797-1.506-1.797Zm-3.703 4.013c-.974 0-1.907.048-2.77.135-.147.015-.241.168-.183.305.483 1.154 1.622 1.964 2.953 1.964 1.33 0 2.47-.81 2.953-1.964.057-.137-.037-.29-.184-.305-.863-.087-1.795-.135-2.769-.135Z",
    github: "M12 .297c-6.63 0-12 5.373-12 12 0 5.303 3.438 9.8 8.205 11.385.6.113.82-.258.82-.577 0-.285-.01-1.04-.015-2.04-3.338.724-4.042-1.61-4.042-1.61C4.422 18.07 3.633 17.7 3.633 17.7c-1.087-.744.084-.729.084-.729 1.205.084 1.838 1.236 1.838 1.236 1.07 1.835 2.809 1.305 3.495.998.108-.776.417-1.305.76-1.605-2.665-.3-5.466-1.332-5.466-5.93 0-1.31.465-2.38 1.235-3.22-.135-.303-.54-1.523.105-3.176 0 0 1.005-.322 3.3 1.23.96-.267 1.98-.399 3-.405 1.02.006 2.04.138 3 .405 2.28-1.552 3.285-1.23 3.285-1.23.645 1.653.24 2.873.12 3.176.765.84 1.23 1.91 1.23 3.22 0 4.61-2.805 5.625-5.475 5.92.42.36.81 1.096.81 2.22 0 1.606-.015 2.896-.015 3.286 0 .315.21.69.825.57C20.565 22.092 24 17.592 24 12.297c0-6.627-5.373-12-12-12",
    forum: "M4 4h16a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H9l-5 4v-4a1 1 0 0 1-1-1V5a1 1 0 0 1 1-1Z",
    blog: "M5 3h14a1 1 0 0 1 1 1v16a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1Zm3 4v2h8V7H8Zm0 4v2h8v-2H8Zm0 4v2h5v-2H8Z",
    bluesky: "M12 10.8c-1.087-2.114-4.046-6.053-6.798-7.995C2.566.944 1.561 1.266.902 1.565.139 1.908 0 3.08 0 3.768c0 .69.378 5.65.624 6.479.815 2.736 3.713 3.66 6.383 3.364.136-.02.275-.039.415-.056-.138.022-.276.04-.415.056-3.912.58-7.387 2.005-2.83 7.078 5.013 5.19 6.87-1.113 7.823-4.308.953 3.195 2.05 9.271 7.733 4.308 4.267-4.308 1.172-6.498-2.74-7.078a8.741 8.741 0 0 1-.415-.056c.14.017.279.036.415.056 2.67.297 5.568-.628 6.383-3.364.246-.828.624-5.79.624-6.478 0-.69-.139-1.861-.902-2.206-.659-.298-1.664-.62-4.3 1.24C16.046 4.748 13.087 8.687 12 10.8Z",
    mastodon: "M23.268 5.313c-.35-2.578-2.617-4.61-5.304-5.004C17.51.242 15.792 0 11.813 0h-.03c-3.98 0-4.835.242-5.288.309C3.882.692 1.496 2.518.917 5.127.64 6.412.61 7.837.661 9.143c.074 1.874.088 3.745.26 5.611.118 1.24.325 2.47.62 3.68.55 2.237 2.777 4.098 4.96 4.857 2.336.792 4.849.923 7.256.38.265-.061.527-.132.786-.213.585-.184 1.27-.39 1.774-.753a.057.057 0 0 0 .023-.043v-1.809a.052.052 0 0 0-.02-.041.053.053 0 0 0-.046-.01 20.282 20.282 0 0 1-4.709.545c-2.73 0-3.463-1.284-3.674-1.818a5.593 5.593 0 0 1-.319-1.433.053.053 0 0 1 .066-.054c1.517.363 3.072.546 4.632.546.376 0 .75 0 1.125-.01 1.57-.044 3.224-.124 4.768-.422.038-.008.077-.015.11-.024 2.435-.464 4.753-1.92 4.989-5.604.008-.145.03-1.52.03-1.67.002-.512.167-3.63-.024-5.545zm-3.748 9.195h-2.561V8.29c0-1.309-.55-1.976-1.67-1.976-1.23 0-1.846.79-1.846 2.35v3.403h-2.546V8.663c0-1.56-.617-2.35-1.848-2.35-1.112 0-1.668.668-1.67 1.977v6.218H4.822V8.102c0-1.31.337-2.35 1.011-3.12.696-.77 1.608-1.164 2.74-1.164 1.311 0 2.302.5 2.962 1.498l.638 1.06.638-1.06c.66-.999 1.65-1.498 2.96-1.498 1.13 0 2.043.395 2.74 1.164.675.77 1.012 1.81 1.012 3.12z",
    instagram: "M7.0301.084c-1.2768.0602-2.1487.264-2.911.5634-.7888.3075-1.4575.72-2.1228 1.3877-.6652.6677-1.075 1.3368-1.3802 2.127-.2954.7638-.4956 1.6365-.552 2.914-.0564 1.2775-.0689 1.6882-.0626 4.947.0062 3.2586.0206 3.6671.0825 4.9473.061 1.2765.264 2.1482.5635 2.9107.308.7889.72 1.4573 1.388 2.1228.6679.6655 1.3365 1.0743 2.1285 1.38.7632.295 1.6361.4961 2.9134.552 1.2773.056 1.6884.069 4.9462.0627 3.2578-.0062 3.668-.0207 4.9478-.0814 1.28-.0607 2.147-.2652 2.9098-.5633.7889-.3086 1.4578-.72 2.1228-1.3881.665-.6682 1.0745-1.3378 1.3795-2.1284.2957-.7632.4966-1.636.552-2.9124.056-1.2809.0692-1.6898.063-4.948-.0063-3.2583-.021-3.6668-.0817-4.9465-.0607-1.2797-.264-2.1487-.5633-2.9117-.3084-.7889-.72-1.4568-1.3876-2.1228C21.2982 1.33 20.628.9208 19.8378.6165 19.074.321 18.2017.1197 16.9244.0645 15.6471.0093 15.236-.005 11.977.0014 8.718.0076 8.31.0215 7.0301.0839m.1402 21.6932c-1.17-.0509-1.8053-.2453-2.2287-.408-.5606-.216-.96-.4771-1.3819-.895-.422-.4178-.6811-.8186-.9-1.378-.1644-.4234-.3624-1.058-.4171-2.228-.0595-1.2645-.072-1.6442-.079-4.848-.007-3.2037.0053-3.583.0607-4.848.05-1.169.2456-1.805.408-2.2282.216-.5613.4762-.96.895-1.3816.4188-.4217.8184-.6814 1.3783-.9003.423-.1651 1.0575-.3614 2.227-.4171 1.2655-.06 1.6447-.072 4.848-.079 3.2033-.007 3.5835.005 4.8495.0608 1.169.0508 1.8053.2445 2.228.408.5608.216.96.4754 1.3816.895.4217.4194.6816.8176.9005 1.3787.1653.4217.3617 1.056.4169 2.2263.0602 1.2655.0739 1.645.0796 4.848.0058 3.203-.0055 3.5834-.061 4.848-.051 1.17-.245 1.8055-.408 2.2294-.216.5604-.4763.96-.8954 1.3814-.419.4215-.8181.6811-1.3783.9-.4224.1649-1.0577.3617-2.2262.4174-1.2656.0595-1.6448.072-4.8493.079-3.2045.007-3.5825-.006-4.848-.0608M16.953 5.5864A1.44 1.44 0 1 0 18.39 4.144a1.44 1.44 0 0 0-1.437 1.4424M5.8385 12.012c.0067 3.4032 2.7706 6.1557 6.173 6.1493 3.4026-.0065 6.157-2.7701 6.1506-6.1733-.0065-3.4032-2.771-6.1565-6.174-6.1498-3.403.0067-6.156 2.771-6.1496 6.1738M8 12.0077a4 4 0 1 1 4.008 3.9921A3.9996 3.9996 0 0 1 8 12.0077",
    tiktok: "M12.525.02c1.31-.02 2.61-.01 3.91-.02.08 1.53.63 3.09 1.75 4.17 1.12 1.11 2.7 1.62 4.24 1.79v4.03c-1.44-.05-2.89-.35-4.2-.97-.57-.26-1.1-.59-1.62-.93-.01 2.92.01 5.84-.02 8.75-.08 1.4-.54 2.79-1.35 3.94-1.31 1.92-3.58 3.17-5.91 3.21-1.43.08-2.86-.31-4.08-1.03-2.02-1.19-3.44-3.37-3.65-5.71-.02-.5-.03-1-.01-1.49.18-1.9 1.12-3.72 2.58-4.96 1.66-1.44 3.98-2.13 6.15-1.72.02 1.48-.04 2.96-.04 4.44-.99-.32-2.15-.23-3.02.37-.63.41-1.11 1.04-1.36 1.75-.21.51-.15 1.07-.14 1.61.24 1.64 1.82 3.02 3.5 2.87 1.12-.01 2.19-.66 2.77-1.61.19-.33.4-.67.41-1.06.1-1.79.06-3.57.07-5.36.01-4.03-.01-8.05.02-12.07z",
    x: "M18.901 1.153h3.68l-8.04 9.19L24 22.846h-7.406l-5.8-7.584-6.638 7.584H.474l8.6-9.83L0 1.154h7.594l5.243 6.932ZM17.61 20.644h2.039L6.486 3.24H4.298Z",
    facebook: "M9.101 23.691v-7.98H6.627v-3.667h2.474v-1.58c0-4.085 1.848-5.978 5.858-5.978.401 0 .955.042 1.468.103a8.68 8.68 0 0 1 1.141.195v3.325a8.623 8.623 0 0 0-.653-.036 26.805 26.805 0 0 0-.733-.009c-.707 0-1.259.096-1.675.309a1.686 1.686 0 0 0-.679.622c-.258.42-.374.995-.374 1.752v1.297h3.919l-.386 2.103-.287 1.564h-3.246v8.245C19.396 23.238 24 18.179 24 12.044c0-6.627-5.373-12-12-12s-12 5.373-12 12c0 5.628 3.874 10.35 9.101 11.647Z",
    discord: "M20.317 4.3698a19.7913 19.7913 0 00-4.8851-1.5152.0741.0741 0 00-.0785.0371c-.211.3753-.4447.8648-.6083 1.2495-1.8447-.2762-3.68-.2762-5.4868 0-.1636-.3933-.4058-.8742-.6177-1.2495a.077.077 0 00-.0785-.037 19.7363 19.7363 0 00-4.8852 1.515.0699.0699 0 00-.0321.0277C.5334 9.0458-.319 13.5799.0992 18.0578a.0824.0824 0 00.0312.0561c2.0528 1.5076 4.0413 2.4228 5.9929 3.0294a.0777.0777 0 00.0842-.0276c.4616-.6304.8731-1.2952 1.226-1.9942a.076.076 0 00-.0416-.1057c-.6528-.2476-1.2743-.5495-1.8722-.8923a.077.077 0 01-.0076-.1277c.1258-.0943.2517-.1923.3718-.2914a.0743.0743 0 01.0776-.0105c3.9278 1.7933 8.18 1.7933 12.0614 0a.0739.0739 0 01.0785.0095c.1202.099.246.1981.3728.2924a.077.077 0 01-.0066.1276 12.2986 12.2986 0 01-1.873.8914.0766.0766 0 00-.0407.1067c.3604.698.7719 1.3628 1.225 1.9932a.076.076 0 00.0842.0286c1.961-.6067 3.9495-1.5219 6.0023-3.0294a.077.077 0 00.0313-.0552c.5004-5.177-.8382-9.6739-3.5485-13.6604a.061.061 0 00-.0312-.0286zM8.02 15.3312c-1.1825 0-2.1569-1.0857-2.1569-2.419 0-1.3332.9555-2.4189 2.157-2.4189 1.2108 0 2.1757 1.0952 2.1568 2.419 0 1.3332-.9555 2.4189-2.1569 2.4189zm7.9748 0c-1.1825 0-2.1569-1.0857-2.1569-2.419 0-1.3332.9554-2.4189 2.1569-2.4189 1.2108 0 2.1757 1.0952 2.1568 2.419 0 1.3332-.946 2.4189-2.1568 2.4189Z",
    other: "M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20Zm-1 2.1V11H4.1A8 8 0 0 1 11 4.1Zm2 0A8 8 0 0 1 19.9 11H13V4.1ZM4.1 13H11v6.9A8 8 0 0 1 4.1 13Zm8.9 0h6.9a8 8 0 0 1-6.9 6.9V13Z"
  };
  const PLATFORMS = { youtube: "YouTube", reddit: "Reddit", github: "GitHub", forum: "Forum", blog: "Blogue", bluesky: "Bluesky",
    mastodon: "Mastodon", instagram: "Instagram", tiktok: "TikTok", x: "X", facebook: "Facebook", discord: "Discord", other: "Autre" };
  const SENTIMENTS = { positive: "Positif", neutral: "Neutre", question: "Question", bug: "Bug", negative: "Négatif" };
  const STATUS = { new: "Nouveau", maybe_replied: "Semble avoir répondu", keeper: "Keeper", seen: "Vu", follow_up: "À suivre", replied: "Répondu", ignored: "Ignoré" };
  const DONE = { maybe_replied: "Semble avoir répondu", keeper: "Keeper", seen: "Vu", follow_up: "À suivre", replied: "Répondu", ignored: "Ignoré", new: "Nouveau" };
  const TABS = [["inbox", "À traiter"], ["maybe_replied", "Semble avoir répondu"], ["seen", "Vus"], ["follow_up", "À suivre"], ["replied", "Répondus"], ["ignored", "Ignorés"], ["keeper", "Keepers"], ["all", "Tout"]];
  const COUNTS = [["new", "Nouveaux", "new"], ["activity", "Nouvelle activité", "new_activity"], ["follow_up", "À suivre", "follow_up"], ["all", "Total", "total"]];
  const LATE = 8 * 3600e3;                 // (a last pass older than this: in red)
  const TOAST_MS = 5000;

  const S = { view: "inbox", platform: "", sentiment: "", priority: "", q: "", items: [], cursor: null, counts: null, picked: new Set(), cur: null, undo: [] };
  const list = $("[data-list]"), state = $("[data-state]"), more = $("[data-more]");
  const say = (t) => { state.textContent = t; state.hidden = !t; };

  function pf(name) {
    const svg = document.createElementNS(NS, "svg");
    svg.setAttribute("viewBox", "0 0 24 24");
    svg.setAttribute("class", "vl-pf");
    svg.setAttribute("data-pf", name);
    svg.setAttribute("aria-hidden", "true");
    const p = document.createElementNS(NS, "path");
    p.setAttribute("d", MARKS[name] || MARKS.other);
    svg.append(p);
    return svg;
  }

  async function api(path, body, method = body ? "PATCH" : "GET") {
    const r = await fetch(path, body ? { method, headers: { "Content-Type": "application/json" }, body: JSON.stringify(body), credentials: "same-origin" }
      : { method, cache: "no-store", credentials: "same-origin" });
    if (r.status === 404 && method === "GET") throw new Error("Pas d'accès.");
    if (r.status === 502) throw new Error("Discord n'a pas accepté le message.");
    if (!r.ok) throw new Error(`Le service a répondu ${r.status}.`);
    return r.json();
  }

  // ------------------------------------------------------------ dates
  // what changes the service goes in the order it was done (an UNDO never overtakes what it undoes)
  let line = Promise.resolve();
  const inTurn = (f) => {
    const p = line.then(f);
    line = p.catch(() => {});
    return p;
  };

  const pad = (n) => String(n).padStart(2, "0");
  const dayOf = (d) => `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`;
  const clock = (t) => [t >= 3600 ? Math.floor(t / 3600) : null, t >= 3600 ? pad(Math.floor(t / 60) % 60) : Math.floor(t / 60), pad(t % 60)].filter((x) => x !== null).join(":");
  function ago(iso) {
    const s = (Date.now() - new Date(iso)) / 1000;
    if (s < 3600) return `il y a ${Math.max(1, Math.floor(s / 60))} min`;
    if (s < 86400) return `il y a ${Math.floor(s / 3600)} h`;
    if (s < 2 * 86400) return "hier";
    if (s < 30 * 86400) return `il y a ${Math.floor(s / 86400)} j`;
    return new Date(iso).toLocaleDateString("fr-CA", { day: "numeric", month: "short", year: "numeric" });
  }
  function dayName(day) {
    const now = new Date(), y = new Date(now.getFullYear(), now.getMonth(), now.getDate() - 1);
    if (day === dayOf(now)) return "Aujourd'hui";
    if (day === dayOf(y)) return "Hier";
    const [a, b, c] = day.split("-").map(Number);
    return new Date(a, b - 1, c).toLocaleDateString("fr-CA", { weekday: "long", day: "numeric", month: "long" });
  }

  // ------------------------------------------------------------ the head
  function counts() {
    const c = S.counts;
    $("[data-counts]").replaceChildren(...COUNTS.map(([view, label, k]) => h("button",
      { class: `vl-count${S.view === view ? " on" : ""}${view !== "all" && view !== "follow_up" && c[k] ? " hot" : ""}`, type: "button", onclick: () => go(view) },
      h("span", { text: label }), h("b", { text: String(c[k]) }))));
    $("[data-tabs]").replaceChildren(...TABS.map(([view, label]) => h("button",
      { class: "vl-tab", type: "button", role: "tab", "aria-selected": String(S.view === view), onclick: () => go(view) }, label,
      h("i", { text: String(view === "all" ? S.tabs.total : S.tabs[view]) }))));
  }
  function run(r) {
    const el = $("[data-run]");
    if (!r) {
      el.className = "vl-run";
      el.textContent = "Aucune veille pour l'instant.";
      return;
    }
    const at = r.finished_at || r.started_at;
    const late = Date.now() - new Date(at) > LATE;
    el.className = `vl-run${late || r.status === "error" ? " bad" : ""}`;
    const n = r.inserted;
    const what = r.status === "error" ? " — en erreur" : r.status ? ` — ${n ? `${n} nouveau${n > 1 ? "x" : ""}` : "rien de nouveau"}` : late ? " — jamais terminée" : " — en cours";
    el.replaceChildren(`Dernière veille : ${ago(at)}${what}${r.status === "partial" ? " (partielle)" : ""}`,
      r.notes && r.status !== "ok" ? h("small", { text: r.notes }) : "");
    el.title = r.notes || "";
  }
  function platforms(all) {
    $("[data-platforms]").replaceChildren(...Object.entries(all).map(([p, n]) => h("button",
      { class: "vl-chip", type: "button", "data-pf": p, "aria-pressed": String(S.platform === p),
        onclick: () => { S.platform = S.platform === p ? "" : p; load(); } },
      pf(p), PLATFORMS[p], h("i", { text: String(n) }))));
  }
  function options() {
    $("[data-sentiment]").replaceChildren(h("option", { value: "", text: "Sentiment" }),
      ...Object.entries(SENTIMENTS).map(([k, v]) => h("option", { value: k, text: v })));
    $("[data-priority]").replaceChildren(h("option", { value: "", text: "Priorité" }),
      ...[3, 2, 1].map((n) => h("option", { value: String(n), text: `Priorité ${n}` })));
  }

  // ------------------------------------------------------------ a card: a video or a thread, the comments under it
  const ORDER = ["new", "maybe_replied", "follow_up", "keeper", "seen", "replied", "ignored"];
  const headOf = (c) => c.root || c.kids[0];                 // (what a note and a post go to)
  const titleOf = (c) => (c.root ? c.root.title : c.kids[0].parent_title || c.kids[0].title);
  const stateOf = (c) => ORDER.find((s) => c.all.some((m) => m.status === s));
  const activeOf = (c) => c.all.some((m) => m.new_activity);
  const whenOf = (m) => (m.published_at ? ago(m.published_at) : `trouvé ${ago(m.first_found_at)}`);      // (its own date unknown: never passed off as one)
  const dots = (n) => "●".repeat(n);
  const out = (href, props, ...kids) => h("a", { href, target: "_blank", rel: "noopener noreferrer", ...props }, ...kids);
  function cards(items) {                                    // (the service's list: each group whole, what it is under first)
    const all = new Map();
    for (const m of items) {
      const c = all.get(m.group) || { key: m.group, day: m.day, root: null, kids: [], all: [] };
      all.set(m.group, c);
      if (m.url === m.group) c.root = m;
      else c.kids.push(m);
      c.all.push(m);
    }
    return [...all.values()];
  }
  function open(c) {
    const ms = c.all.filter((m) => m.status === "new");
    if (!ms.length) return;
    for (const m of ms) m.status = "seen";
    c.stays = true;
    paint(c);
    send(ms, "seen", false).then((d) => shown(d.counts)).catch(() => {
      for (const m of ms) m.status = "new";
      paint(c);
    });
  }
  const opens = (c) => ({ onclick: () => open(c), onauxclick: (e) => { if (e.button === 1) open(c); } });
  const said = (m) => h("p", { class: `vl-sum${m.quote ? " said" : ""}`, text: m.quote || m.summary });     // (their words; ours failing that)
  function comment(c, m) {
    return h("div", { class: `vl-kid${m.is_own ? " own" : ""}${m.priority === 3 ? " p3" : ""}` },
      h("p", { class: "vl-by" }, out(m.url, { text: m.author || "Commentaire", ...opens(c) }), ` · ${whenOf(m)}`,
        m.is_own && h("span", { class: "vl-tag own", text: "Toi" }),
        h("span", { class: `vl-tag s ${m.sentiment}`, text: SENTIMENTS[m.sentiment] }),
        h("span", { class: "vl-tag vl-p", title: `Priorité ${m.priority}`, text: dots(m.priority) }),
        m.new_activity && h("span", { class: "vl-tag lit", text: "Nouvelle activité" })),
      said(m));
  }
  function paint(c) {
    const r = c.root, t = headOf(c), n = c.kids.length, state = stateOf(c), el = c.el;
    const by = r ? [r.author, r.community === r.author ? null : r.community, whenOf(r)] : [t.community, `${n} commentaire${n > 1 ? "s" : ""}`];
    const note = c.noteOpen && h("textarea", { class: "vl-note", "aria-label": "Note", oninput: (e) => noteSoon(c, e.target.value),
      onblur: (e) => { noteNow(c, e.target.value); }, onkeydown: (e) => { if (e.key === "Escape") closeNote(c); } });
    if (note) note.value = t.notes;
    el.className = `vl-card${c.all.every((m) => m.is_own) ? " own" : ""}${c.all.some((m) => m.priority === 3) ? " p3" : ""}${S.picked.has(c.key) ? " sel" : ""}${S.cur === c ? " cur" : ""}`;
    el.replaceChildren(
      h("label", { class: "vl-tick" }, h("span", { class: "sr-only", text: "Sélectionner" }),
        h("input", { type: "checkbox", checked: S.picked.has(c.key), onclick: (e) => pick(c, e.target.checked, e.shiftKey) })),
      h("div", { class: "vl-b" },
        h("div", { class: "vl-t" }, pf(t.platform), out(c.key, { text: titleOf(c), ...opens(c) })),
        h("p", { class: "vl-by", text: by.filter(Boolean).join(" · ") }),
        r && said(r),
        r && r.moments.length > 0 && h("div", { class: "vl-moments" }, r.moments.map((x) => out(x.url, opens(c),
          h("b", { text: clock(x.at) }), x.quote && h("span", { text: x.quote })))),
        h("div", { class: "vl-tags" },
          r && h("span", { class: `vl-tag s ${r.sentiment}`, text: SENTIMENTS[r.sentiment] }),
          r && h("span", { class: "vl-tag vl-p", title: `Priorité ${r.priority}`, text: dots(r.priority) }),
          h("span", { class: "vl-tag", text: t.language }),
          r && n > 0 && h("span", { class: "vl-tag", text: `${n} commentaire${n > 1 ? "s" : ""}` }),
          r && r.new_activity && h("span", { class: "vl-tag lit", text: "Nouvelle activité" }),
          r && r.is_own && h("span", { class: "vl-tag own", text: "Toi" }),
          state !== "new" && h("span", { class: `vl-state${state === "maybe_replied" ? " maybe" : ""}`, text: STATUS[state] })),
        n > 0 && h("div", { class: "vl-kids" }, c.kids.map((m) => comment(c, m))),
        !c.noteOpen && t.notes && h("p", { class: "vl-note-v", text: t.notes }),
        note,
        h("div", { class: "vl-do" },
          ...[["replied", "Répondu"], ["follow_up", "À suivre"], ["ignored", "Ignorer"], ["keeper", "Keeper"]].map(([s, label]) => h("button",
            { class: "vl-act", type: "button", "aria-pressed": String(state === s), onclick: () => act([c], s) }, label)),
          h("button", { class: "vl-act", type: "button", "aria-expanded": String(!!c.noteOpen), onclick: () => (c.noteOpen ? closeNote(c) : openNote(c)) }, "Note"),
          S.discord && (t.discord_url
            ? out(t.discord_url, { class: "vl-act", title: "Le voir dans #made-with-frasil", text: "Posté ↗" })
            : h("button", { class: "vl-act", type: "button", title: "Poster dans #made-with-frasil", disabled: c.posting, onclick: () => post(c) }, "Discord")))));
    return note;
  }
  function card(c) {
    if (!c.el) {
      c.el = h("article", { "data-pf": headOf(c).platform, tabindex: "-1", onmousedown: () => here(c, false) });
      paint(c);
    }
    return c.el;
  }
  const rows = () => S.items.filter((c) => !c.gone);
  function draw() {
    const shown = rows(), all = [];
    let day;
    for (const c of shown) {
      if (c.day !== day) {
        day = c.day;
        all.push(h("h2", { class: "vl-day" }, dayName(day), h("i", { text: String(shown.filter((x) => x.day === day).length) })));
      }
      all.push(card(c));
    }
    list.replaceChildren(...(all.length ? all : [empty()]));
    more.hidden = !S.cursor;
    if (!S.cur || S.cur.gone || !shown.includes(S.cur)) here(shown[0] || null, false);
    picks();
  }
  function empty() {
    const quiet = S.view === "inbox" && !S.platform && !S.sentiment && !S.priority && !S.q;
    const lake = document.createElementNS(NS, "svg");
    lake.setAttribute("viewBox", "0 0 150 54");
    lake.setAttribute("aria-hidden", "true");
    lake.innerHTML = '<path d="M2 30h146" stroke="var(--mist)" stroke-width="1" fill="none"/><path d="M26 39h98M48 47h54" stroke="var(--steel)" stroke-width="1" fill="none"/>' +
      '<path d="M58 30a17 17 0 0 1 34 0Z" fill="var(--lamp)"/>';
    return h("div", { class: "vl-empty" }, lake, h("b", { text: quiet ? "Rien de neuf, tout est traité" : "Rien ici" }),
      h("span", { text: quiet ? "La prochaine veille passera d'elle-même." : "Aucune mention ici." }));
  }

  // ------------------------------------------------------------ the current card, the picked ones
  function here(m, show = true) {
    const was = S.cur;
    S.cur = m;
    if (was && was.el) was.el.classList.remove("cur");
    if (m && m.el) {
      m.el.classList.add("cur");
      if (show) {
        m.el.focus({ preventScroll: true });
        m.el.scrollIntoView({ block: "nearest" });
      }
    }
  }
  function step(d) {
    const all = rows(), i = all.indexOf(S.cur);
    const m = all[Math.min(all.length - 1, Math.max(0, i + d))];
    if (m) here(m);
    if (d > 0 && i >= all.length - 3 && S.cursor) page();
  }
  function mark(c, on) {
    if (on) S.picked.add(c.key);
    else S.picked.delete(c.key);
    paint(c);
  }
  function pick(c, on, range) {
    const all = rows();
    if (range && S.anchor && all.includes(S.anchor)) {
      const [a, b] = [all.indexOf(S.anchor), all.indexOf(c)].sort((x, y) => x - y);
      for (const x of all.slice(a, b + 1)) mark(x, on);
    } else mark(c, on);
    S.anchor = c;
    picks();
  }
  function unpick() {
    const was = S.items.filter((c) => S.picked.has(c.key));
    S.picked.clear();
    was.forEach(paint);
    picks();
  }
  function picks() {
    for (const k of S.picked) if (!rows().some((c) => c.key === k)) S.picked.delete(k);
    const n = S.picked.size;
    $("[data-bulk]").hidden = !n;
    $("[data-bulk-n]").replaceChildren(String(n), h("span", { text: ` sélectionnée${n > 1 ? "s" : ""}` }));
    $("[data-toast]").classList.toggle("has-bulk", !!n);
  }

  // ------------------------------------------------------------ what is done with them: a card, all of it
  const fits = (m) => S.view === "all" || (S.view === "inbox" ? m.status === "new" || m.new_activity
    : S.view === "activity" ? m.new_activity : m.status === S.view);
  const belongs = (c) => c.stays || c.all.some(fits);
  function leaving(cs) {
    const gone = cs.filter((c) => !belongs(c));
    if (!gone.length) return;
    const next = rows().slice(rows().indexOf(S.cur)).find((c) => !gone.includes(c)) || [...rows()].reverse().find((c) => !gone.includes(c));
    if (gone.includes(S.cur)) here(next || null);
    for (const c of gone) c.el.classList.add("out");
    setTimeout(() => {
      for (const c of gone) c.gone = !belongs(c);
      draw();
    }, matchMedia("(prefers-reduced-motion: reduce)").matches ? 0 : 160);
  }
  function send(ms, status, activity) {
    const ids = ms.map((m) => m.id);
    return inTurn(() => (ids.length === 1 ? api(`/api/veille/mentions/${ids[0]}`, { status, new_activity: activity })
      : api("/api/veille/mentions", { ids, status, new_activity: activity })));
  }
  function act(cs, status) {
    if (!cs.length) return;
    const ms = cs.flatMap((c) => c.all);
    const before = { cs: cs.map((c) => ({ c, stays: c.stays })), ms: ms.map((m) => ({ m, status: m.status, new_activity: m.new_activity })) };
    const next = cs.length === 1 && ms.every((m) => m.status === status && !m.new_activity) ? "seen" : status;
    for (const m of ms) Object.assign(m, { status: next, new_activity: false });
    for (const c of cs) c.stays = false;
    S.picked.clear();
    S.items.forEach((c) => c.el && paint(c));
    picks();
    leaving(cs);
    const back = () => restore(before);
    S.undo.push(back);
    toast(cs.length > 1 ? `${cs.length} mentions : ${DONE[next].toLowerCase()}` : `${DONE[next]} : ${titleOf(cs[0])}`, back);
    send(ms, next, false).then((d) => shown(d.counts)).catch((e) => {
      const i = S.undo.indexOf(back);
      if (i >= 0) S.undo.splice(i, 1);
      revert(before);
      toast(e.message);
    });
  }
  function revert(before) {
    for (const b of before.ms) Object.assign(b.m, { status: b.status, new_activity: b.new_activity });
    for (const b of before.cs) {
      Object.assign(b.c, { stays: b.stays, gone: false });
      if (b.c.el) paint(b.c);
    }
    draw();
  }
  function restore(before) {
    const elsewhere = before.cs.some((b) => !S.items.includes(b.c));     // (another tab since: the list comes back from the service)
    revert(before);
    const groups = new Map();
    for (const b of before.ms) {
      const k = `${b.status}|${b.new_activity}`;
      groups.set(k, [...(groups.get(k) || []), b]);
    }
    toast("Annulé");
    Promise.all([...groups.values()].map((g) => send(g.map((b) => b.m), g[0].status, g[0].new_activity)))
      .then((all) => (elsewhere ? load() : shown(all[all.length - 1].counts))).catch((e) => {
        toast(e.message);
        load();
      });
  }
  function undo() {
    const back = S.undo.pop();
    if (back) back();
  }
  function shown(c) {
    if (c) S.counts = c;
    if (!S.platform && !S.sentiment && !S.priority && !S.q) S.tabs = S.counts;
    else api(query(null, 1)).then((d) => { S.tabs = d.tabs; counts(); }).catch(() => {});
    counts();
  }

  let toastTimer;
  function toast(text, undoIt) {
    const el = $("[data-toast]");
    clearTimeout(toastTimer);
    el.replaceChildren(h("span", { text }), ...(undoIt ? [h("button", { type: "button", text: "Annuler", onclick: undo })] : []));
    el.hidden = false;
    toastTimer = setTimeout(() => { el.hidden = true; }, TOAST_MS);
  }

  // ------------------------------------------------------------ into #made-with-frasil (the service's bot)
  async function post(c) {
    const t = headOf(c);
    if (!S.discord || t.discord_url || c.posting) return;
    c.posting = true;
    paint(c);
    try {
      const d = await inTurn(() => api(`/api/veille/mentions/${t.id}/discord`, {}, "POST"));
      t.discord_url = d.item.discord_url;
      const back = () => unpost(c);
      S.undo.push(back);
      toast("Posté dans #made-with-frasil", back);
    } catch (e) {
      toast(e.message);
    }
    c.posting = false;
    paint(c);
  }
  async function unpost(c) {
    const t = headOf(c), was = t.discord_url;
    t.discord_url = null;
    paint(c);
    toast("Retiré de Discord");
    try {
      await inTurn(() => api(`/api/veille/mentions/${t.id}/discord`, undefined, "DELETE"));
      if (!S.items.includes(c)) load();           // (another list since)
    } catch (e) {
      t.discord_url = was;
      paint(c);
      S.undo.push(() => unpost(c));
      toast(e.message);
    }
  }

  // ------------------------------------------------------------ notes (a card's, on what it is about)
  function openNote(c) {
    c.noteOpen = true;
    paint(c);
    $(".vl-note", c.el).focus();
  }
  function closeNote(c) {
    const t = $(".vl-note", c.el);
    if (t) noteNow(c, t.value);
    c.noteOpen = false;
    paint(c);
    here(c);
  }
  function noteNow(c, text) {
    clearTimeout(c.noteTimer);
    const t = headOf(c);
    if (text === t.notes) return;
    const was = t.notes;
    t.notes = text;
    inTurn(() => api(`/api/veille/mentions/${t.id}`, { notes: text })).catch((e) => {
      t.notes = was;
      if (!c.noteOpen) paint(c);                  // (what is being typed stays there, sent again with the next change)
      toast(e.message);
    });
  }
  function noteSoon(c, text) {
    clearTimeout(c.noteTimer);
    c.noteTimer = setTimeout(() => noteNow(c, text), 600);
  }

  // ------------------------------------------------------------ the list from the service
  function query(cursor, limit) {
    const q = new URLSearchParams({ status: S.view, tz: String(new Date().getTimezoneOffset()) });
    if (limit) q.set("limit", String(limit));
    for (const k of ["platform", "sentiment", "priority", "q"]) if (S[k]) q.set(k, S[k]);
    if (cursor) q.set("cursor", cursor);
    return `/api/veille/mentions?${q}`;
  }
  let asked = 0;
  async function load() {
    const me = ++asked;
    S.cursor = null;
    S.paging = false;
    try {
      const d = await api(query());
      if (me !== asked) return;
      S.items = cards(d.items);
      S.cursor = d.next_cursor;
      S.cur = null;
      S.picked.clear();
      S.counts = d.counts;
      S.tabs = d.tabs;
      S.discord = d.discord;
      counts();
      run(d.last_run);
      platforms(d.platforms);
      $("[data-top]").hidden = $("[data-filters]").hidden = false;
      say("");
      draw();
    } catch (e) {
      if (me === asked) say(e.message);
    }
  }
  async function page() {
    if (!S.cursor || S.paging) return;
    const me = asked;
    S.paging = true;
    try {
      const d = await api(query(S.cursor));
      if (me !== asked) return;                   // (another list since)
      S.items.push(...cards(d.items).filter((c) => !S.items.some((x) => x.key === c.key)));
      S.cursor = d.next_cursor;
      draw();
    } catch (e) {
      if (me === asked) toast(e.message);
    }
    if (me === asked) S.paging = false;
  }
  function go(view) {
    S.view = view;
    load();
  }

  // ------------------------------------------------------------ the keys, the rest
  document.addEventListener("keydown", (e) => {
    if (e.metaKey || e.ctrlKey || e.altKey || (e.repeat && e.key !== "j" && e.key !== "k")) return;
    if (e.target.closest("input, textarea, select")) {
      if (e.key === "Escape") e.target.blur();
      return;
    }
    if (e.key === "Escape" && S.picked.size) return unpick();
    const m = S.cur;
    const did = { j: () => step(1), k: () => step(-1), u: undo, "/": () => $("[data-find]").focus(),
      o: () => m && $(".vl-t a", m.el).click(), r: () => m && act(targets(m), "replied"), f: () => m && act(targets(m), "follow_up"),
      i: () => m && act(targets(m), "ignored"), h: () => m && act(targets(m), "keeper"), n: () => m && openNote(m), d: () => m && post(m), x: () => m && pick(m, !S.picked.has(m.key), e.shiftKey) }[e.key];
    if (!did) return;
    e.preventDefault();
    did();
  });
  const targets = (c) => (S.picked.size ? S.items.filter((x) => S.picked.has(x.key)) : [c]);
  for (const b of document.querySelectorAll("[data-bulk-act]")) {
    b.addEventListener("click", () => act(S.items.filter((c) => S.picked.has(c.key)), b.dataset.bulkAct));
  }
  $("[data-bulk-all]").addEventListener("click", () => {
    for (const c of rows()) { S.picked.add(c.key); paint(c); }
    picks();
  });
  $("[data-bulk-none]").addEventListener("click", unpick);
  more.addEventListener("click", page);
  new IntersectionObserver((seen) => { if (seen.some((x) => x.isIntersecting)) page(); }, { rootMargin: "300px" }).observe(more);
  let typing;
  $("[data-find]").addEventListener("input", (e) => {
    clearTimeout(typing);
    typing = setTimeout(() => { S.q = e.target.value.trim(); load(); }, 220);
  });
  for (const k of ["sentiment", "priority"]) $(`[data-${k}]`).addEventListener("change", (e) => { S[k] = e.target.value; load(); });
  options();
  load();
})();
