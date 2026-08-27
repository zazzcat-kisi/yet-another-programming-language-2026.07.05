// Measure token cost of a standard LLM coding-activity bundle over the sampled corpus.
const fs = require('fs'), path = require('path');
const {encode: encO} = require('gpt-tokenizer/encoding/o200k_base');
const {encode: encC} = require('gpt-tokenizer/encoding/cl100k_base');
const {cfgFor, slocCount, splitCode} = require('./langmeta.js');

const CORPUS = process.argv[2];
const MANIFESTS = process.argv.slice(3, -1);
const OUT = process.argv[process.argv.length - 1];

// slug -> display name
const names = {};
for (const mf of MANIFESTS){
  for (const line of fs.readFileSync(mf,'utf8').split('\n')){
    const c = line.split('\t');
    if (c.length < 3 || c[0]==='name' || !c[0]) continue;
    const slug = c[0].toLowerCase().replace(/#/g,'sharp').replace(/\+/g,'p').replace(/[^a-z0-9]+/g,'_').replace(/^_|_$/g,'');
    names[slug] = {name: c[0], kind: c[1], repo: c[2]};
  }
}

const T = s => encO(s).length;

function window_(lines, frac, n){
  const start = Math.max(0, Math.min(lines.length - n, Math.floor(lines.length*frac)));
  return lines.slice(start, start+n).join('\n');
}
// Synthetic unified diff: 12-line hunk, 3 lines replaced (shown as -/+).
function diffHunk(lines, frac, ctx=12, changed=3){
  const start = Math.max(0, Math.min(lines.length-ctx, Math.floor(lines.length*frac)));
  const h = lines.slice(start, start+ctx);
  const mid = Math.floor((ctx-changed)/2);
  const out = ['@@ -'+(start+1)+','+ctx+' +'+(start+1)+','+ctx+' @@'];
  h.forEach((l,i)=>{
    if (i>=mid && i<mid+changed){ out.push('-'+l); out.push('+'+l); }
    else out.push(' '+l);
  });
  return out.join('\n');
}
function sampleLines(lines, n){
  const step = Math.max(1, Math.floor(lines.length/n));
  const out=[]; for (let i=0;i<lines.length && out.length<n;i+=step) out.push(lines[i]);
  return out.join('\n');
}

// Standard session over a 300-line slice: full read + 2 focused reads (120 lines)
// + 1 search digest (60 lines) + 2 patches (12-line hunks) + 1 new 40-line function.
function session(text){
  const lines = text.split('\n');
  const SLICE = 300;
  if (lines.length < SLICE) return null;
  const mid = Math.floor((lines.length-SLICE)/2);
  const slice = lines.slice(mid, mid+SLICE);
  const s = slice.join('\n');
  const parts = {
    read_full: T(s),
    focus1: T(window_(slice,0.20,120)),
    focus2: T(window_(slice,0.60,120)),
    grep:   T(sampleLines(slice,60)),
    patch1: T(diffHunk(slice,0.30)),
    patch2: T(diffHunk(slice,0.70)),
    newfn:  T(window_(slice,0.45,40)),
  };
  parts.total = Object.values(parts).reduce((a,b)=>a+b,0);
  return parts;
}

const rows = [];
for (const kind of fs.readdirSync(CORPUS)){
  const kdir = path.join(CORPUS, kind);
  if (!fs.statSync(kdir).isDirectory()) continue;
  for (const slug of fs.readdirSync(kdir)){
    const dir = path.join(kdir, slug);
    if (!fs.statSync(dir).isDirectory()) continue;
    const meta = names[slug] || {name: slug, kind, repo: ''};
    for (const f of fs.readdirSync(dir)){
      if (f.startsWith('.')) continue;
      const p = path.join(dir, f);
      let text;
      try { text = fs.readFileSync(p,'utf8'); } catch(e){ continue; }
      if (!text.trim()) continue;
      const ext = f.split('.').pop();
      const cfg = cfgFor(meta.name, ext);
      const sc = slocCount(text, cfg);
      const to = encO(text).length, tc = encC(text).length;
      const sp = splitCode(text, cfg);
      const tokCode = sp.code ? encO(sp.code).length : 0;
      const tokComment = sp.comments ? encO(sp.comments).length : 0;
      const ses = session(text);
      rows.push({
        kind: meta.kind, name: meta.name, slug, file: f, repo: meta.repo,
        bytes: Buffer.byteLength(text), chars: text.length,
        lines: sc.lines, nonblank: sc.nonblank, sloc: sc.sloc, comment: sc.comment,
        tok_o200k: to, tok_cl100k: tc, tok_code: tokCode, tok_comment: tokComment,
        chars_code: sp.code.length,
        session_total: ses ? ses.total : '', session_read: ses ? ses.read_full : '',
        session_patch: ses ? (ses.patch1+ses.patch2) : '',
      });
    }
  }
}
const cols = Object.keys(rows[0]);
fs.writeFileSync(OUT, cols.join(',')+'\n'+rows.map(r=>cols.map(c=>{
  const v = String(r[c]); return /[",]/.test(v) ? '"'+v.replace(/"/g,'""')+'"' : v;
}).join(',')).join('\n')+'\n');
console.log('wrote', OUT, rows.length, 'files');
