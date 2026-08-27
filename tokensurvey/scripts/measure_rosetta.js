// Functionality-normalised token cost: same Rosetta Code tasks implemented in each language.
const fs = require('fs'), path = require('path');
const {encode} = require('gpt-tokenizer/encoding/o200k_base');
const {cfgFor, slocCount} = require('./langmeta.js');

const ROOT = process.argv[2];              // /tmp/rcd/Lang
const OUT  = process.argv[3];              // rosetta_languages.csv
const OUTM = process.argv[4];              // rosetta_matrix.csv

// SO-2025 language -> Rosetta Code language directory
const MAP = {
  'Ada':'Ada','Apex':'Apex','Assembly':'X86-Assembly','Bash/Shell':'UNIX-Shell','C':'C',
  'C#':'C-sharp','C++':'C++','Clojure':'Clojure','Cobol':'COBOL','Crystal':'Crystal',
  'Dart':'Dart','Delphi':'Delphi','Elixir':'Elixir','Erlang':'Erlang','F#':'F-Sharp',
  'Fortran':'Fortran','GDScript':'GDScript','Gleam':'Gleam','Go':'Go','Groovy':'Groovy',
  'Haskell':'Haskell','Java':'Java','JavaScript':'JavaScript','Julia':'Julia','Kotlin':'Kotlin',
  'Lisp':'Common-Lisp','Lua':'Lua','MATLAB':'MATLAB','MicroPython':'Python','Nim':'Nim',
  'OCaml':'OCaml','Objective-C':'Objective-C','PHP':'PHP','Perl':'Perl','PowerShell':'PowerShell',
  'Prolog':'Prolog','Python':'Python','R':'R','Ruby':'Ruby','Rust':'Rust','Scala':'Scala',
  'SQL':'SQL','Swift':'Swift','Tcl':'Tcl','TypeScript':'TypeScript','VBA':'VBA',
  'Visual Basic (.Net)':'Visual-Basic-.NET','Zig':'Zig',
};

const MIN_TOK = 25, MAX_TOK = 2000;
const langTasks = {};   // lang -> task -> {tok, lines, chars, sloc}

for (const [lang, dir] of Object.entries(MAP)){
  const base = path.join(ROOT, dir);
  if (!fs.existsSync(base)) { console.error('missing', dir); continue; }
  const m = {};
  for (const task of fs.readdirSync(base)){
    const td = path.join(base, task);
    let st; try { st = fs.statSync(td); } catch(e){ continue; }
    if (!st.isDirectory()) continue;
    const files = fs.readdirSync(td).filter(f=>!f.startsWith('.'))
      .map(f=>({f, p:path.join(td,f)}))
      .filter(o=>{ try { return fs.statSync(o.p).isFile(); } catch(e){ return false; } });
    if (!files.length) continue;
    // representative implementation = median-sized solution for that task
    files.forEach(o=>{ o.size = fs.statSync(o.p).size; });
    files.sort((a,b)=>a.size-b.size);
    const STRAT = process.env.REP_STRATEGY || 'median';
    const rep = STRAT==='largest' ? files[files.length-1]
              : STRAT==='smallest' ? files[0]
              : STRAT==='first' ? [...files].sort((a,b)=>a.f.localeCompare(b.f))[0]
              : files[Math.floor(files.length/2)];
    let text; try { text = fs.readFileSync(rep.p,'utf8'); } catch(e){ continue; }
    if (!text.trim()) continue;
    const tok = encode(text).length;
    if (tok < MIN_TOK || tok > MAX_TOK) continue;
    const ext = rep.f.split('.').pop();
    const sc = slocCount(text, cfgFor(lang, ext));
    m[task] = {tok, lines: sc.lines, sloc: sc.sloc, chars: text.length};
  }
  langTasks[lang] = m;
  console.error(lang, Object.keys(m).length, 'tasks');
}

const langs = Object.keys(langTasks);
// task coverage across languages
const cov = {};
for (const l of langs) for (const t of Object.keys(langTasks[l])) cov[t] = (cov[t]||0)+1;
const NEED = Math.ceil(langs.length * 0.75);
const common = Object.keys(cov).filter(t=>cov[t] >= NEED).sort();
console.error('languages:', langs.length, 'common tasks (>=' + NEED + ' langs):', common.length);

// per-task medians across languages
const med = arr => { const a=[...arr].sort((x,y)=>x-y); const n=a.length;
  return n%2 ? a[(n-1)/2] : (a[n/2-1]+a[n/2])/2; };
const taskMedTok = {}, taskMedLines = {};
for (const t of common){
  const tk=[], ln=[];
  for (const l of langs){ const e = langTasks[l][t]; if (e){ tk.push(e.tok); ln.push(e.lines); } }
  taskMedTok[t] = med(tk); taskMedLines[t] = med(ln);
}
const gm = a => Math.exp(a.reduce((s,x)=>s+Math.log(x),0)/a.length);
const baseTok = gm(common.map(t=>taskMedTok[t]));
const baseLines = gm(common.map(t=>taskMedLines[t]));

const rows = [];
const matrix = [];
for (const l of langs){
  const rt=[], rl=[]; let chars=0, toks=0, n=0;
  for (const t of common){
    const e = langTasks[l][t]; if (!e) continue;
    rt.push(e.tok/taskMedTok[t]); rl.push(e.lines/taskMedLines[t]);
    chars += e.chars; toks += e.tok; n++;
    matrix.push([l,t,e.tok,e.lines,e.sloc,e.chars].join(','));
  }
  if (n < 12) { console.error('SKIP (low coverage)', l, n); continue; }
  rows.push({
    language: l, rosetta_dir: MAP[l], tasks: n,
    tok_index: +(gm(rt)).toFixed(4),
    tokens_per_task: +(gm(rt)*baseTok).toFixed(1),
    lines_index: +(gm(rl)).toFixed(4),
    lines_per_task: +(gm(rl)*baseLines).toFixed(1),
    chars_per_token: +(chars/toks).toFixed(3),
  });
}
rows.sort((a,b)=>a.tokens_per_task-b.tokens_per_task);
const cols = Object.keys(rows[0]);
fs.writeFileSync(OUT, cols.join(',')+'\n'+rows.map(r=>cols.map(c=>r[c]).join(',')).join('\n')+'\n');
fs.writeFileSync(OUTM, 'language,task,tokens,lines,sloc,chars\n'+matrix.join('\n')+'\n');
console.error('base tokens/task median-of-medians:', baseTok.toFixed(1), 'lines:', baseLines.toFixed(1));
console.log('wrote', OUT, rows.length, 'languages;', matrix.length, 'implementations');
