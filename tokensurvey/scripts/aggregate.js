const fs = require('fs');
const D = 'data/';
function readCsv(p){
  const [h,...ls] = fs.readFileSync(p,'utf8').trim().split('\n');
  const cols = h.split(',');
  return ls.map(l=>{
    const v=[]; let cur='', q=false;
    for (let i=0;i<l.length;i++){ const c=l[i];
      if (q){ if (c==='"'){ if (l[i+1]==='"'){cur+='"';i++;} else q=false; } else cur+=c; }
      else if (c==='"') q=true; else if (c===','){ v.push(cur); cur=''; } else cur+=c; }
    v.push(cur);
    const o={}; cols.forEach((c,i)=>{ const x=v[i]; o[c] = (x!=='' && !isNaN(Number(x))) ? Number(x) : x; });
    return o;
  });
}
const med = a => { if(!a.length) return null; const s=[...a].sort((x,y)=>x-y), n=s.length;
  return n%2 ? s[(n-1)/2] : (s[n/2-1]+s[n/2])/2; };
const sum = a => a.reduce((x,y)=>x+y,0);
const r1 = x => x==null? null : Math.round(x*10)/10;
const r2 = x => x==null? null : Math.round(x*100)/100;

const files = readCsv(D+'corpus_files.csv');
const ros = {}; readCsv(D+'rosetta_languages.csv').forEach(r=>ros[r.language]=r);

function agg(kind){
  const by = {};
  files.filter(f=>f.kind===kind).forEach(f=>{ (by[f.name]=by[f.name]||[]).push(f); });
  return Object.entries(by).map(([name,fs_])=>{
    const ses = fs_.map(f=>f.session_total).filter(x=>typeof x==='number' && x>0);
    return {
      name, files: fs_.length, repos: [...new Set(fs_.map(f=>f.repo))].join(' + '),
      chars: sum(fs_.map(f=>f.chars)), lines: sum(fs_.map(f=>f.lines)),
      sloc: sum(fs_.map(f=>f.sloc)), comments: sum(fs_.map(f=>f.comment)),
      tok: sum(fs_.map(f=>f.tok_o200k)), tok_cl: sum(fs_.map(f=>f.tok_cl100k)),
      tok_code: sum(fs_.map(f=>f.tok_code)), tok_comment: sum(fs_.map(f=>f.tok_comment)),
      chars_code: sum(fs_.map(f=>f.chars_code)),
      med_tok_file: med(fs_.map(f=>f.tok_o200k)),
      med_lines_file: med(fs_.map(f=>f.lines)),
      session: ses.length>=3 ? med(ses) : null, session_n: ses.length,
    };
  }).map(o=>({
    ...o,
    chars_per_token: r2(o.chars/o.tok),
    code_chars_per_token: r2(o.chars_code/o.tok_code),
    tokens_per_line: r2(o.tok/o.lines),
    tokens_per_sloc: r2(o.tok_code/o.sloc),
    comment_token_share: r2(o.tok_comment/o.tok),
    cl_ratio: r2(o.tok_cl/o.tok),
  }));
}

// ---------- languages ----------
let L = agg('lang').map(o=>{
  const r = ros[o.name];
  return {...o,
    tokens_per_task: r? r.tokens_per_task : null,
    lines_per_task: r? r.lines_per_task : null,
    rosetta_tasks: r? r.tasks : 0,
    rc_chars_per_token: r? r.chars_per_token : null,
  };
});
const ranked = L.filter(o=>o.tokens_per_task).map(o=>o.tokens_per_task);
const baseline = med(ranked);
L.forEach(o=>{ o.tei = o.tokens_per_task ? Math.round(100*baseline/o.tokens_per_task) : null; });
L.sort((a,b)=> (a.tokens_per_task||1e9)-(b.tokens_per_task||1e9) || a.name.localeCompare(b.name));
L.forEach((o,i)=>{ o.rank = o.tokens_per_task ? i+1 : ''; });

// ---------- frameworks ----------
const hosts = {};
fs.readFileSync(D+'framework_hosts.tsv','utf8').trim().split('\n').forEach(l=>{
  const [f,h] = l.split('\t'); hosts[f] = (h||'').trim();
});
const langDensity = {}; L.forEach(o=>langDensity[o.name]=o.tokens_per_sloc);
let F = agg('fw').map(o=>{
  const h = hosts[o.name] || '';
  const base = langDensity[h];
  return {...o, host_language: h,
    vs_host_language: base ? r2(o.tokens_per_sloc/base) : null};
});
const fwBase = med(F.map(o=>o.tokens_per_sloc));
F.forEach(o=>{ o.tei = Math.round(100*fwBase/o.tokens_per_sloc); });
F.sort((a,b)=>a.tokens_per_sloc-b.tokens_per_sloc);
F.forEach((o,i)=>o.rank=i+1);

function write(rows, cols, p){
  fs.writeFileSync(p, cols.join(',')+'\n'+rows.map(r=>cols.map(c=>{
    const v = r[c]==null?'':String(r[c]); return /[",]/.test(v)?'"'+v.replace(/"/g,'""')+'"':v;
  }).join(',')).join('\n')+'\n');
}
const LC = ['rank','name','tei','tokens_per_task','lines_per_task','rosetta_tasks','tokens_per_sloc','tokens_per_line','chars_per_token','code_chars_per_token','rc_chars_per_token','med_tok_file','med_lines_file','session','files','sloc','lines','tok','comment_token_share','cl_ratio','repos'];
const FC = ['rank','name','tei','tokens_per_sloc','host_language','vs_host_language','tokens_per_line','chars_per_token','code_chars_per_token','med_tok_file','med_lines_file','session','files','sloc','lines','tok','comment_token_share','cl_ratio','repos'];
write(L, LC, D+'languages_summary.csv');
write(F, FC, D+'frameworks_summary.csv');

// Spearman between o200k and cl100k language rankings (tokens_per_sloc)
function spearman(a,b){
  const rk = arr => { const idx = arr.map((v,i)=>[v,i]).sort((x,y)=>x[0]-y[0]); const r=[];
    idx.forEach((p,i)=>r[p[1]]=i+1); return r; };
  const ra=rk(a), rb=rk(b), n=a.length;
  const d2 = sum(ra.map((v,i)=>(v-rb[i])**2));
  return 1 - 6*d2/(n*(n*n-1));
}
const la = agg('lang');
console.log('Spearman o200k vs cl100k (tokens/SLOC, languages):',
  spearman(la.map(o=>o.tok/o.sloc), la.map(o=>o.tok_cl/o.sloc)).toFixed(4));
console.log('baseline median tokens/task:', baseline, '| fw baseline tokens/sloc:', fwBase);
const both = L.filter(o=>o.tokens_per_task);
console.log('Spearman tokens/task (Rosetta) vs tokens/SLOC (real corpus):',
  spearman(both.map(o=>o.tokens_per_task), both.map(o=>o.tokens_per_sloc)).toFixed(3));
console.log('Spearman tokens/task vs lines/task:',
  spearman(both.map(o=>o.tokens_per_task), both.map(o=>o.lines_per_task)).toFixed(3));
console.log('Spearman tokens/task vs corpus chars-per-token:',
  spearman(both.map(o=>o.tokens_per_task), both.map(o=>-o.chars_per_token)).toFixed(3));
console.log('languages:', L.length, 'frameworks:', F.length);
