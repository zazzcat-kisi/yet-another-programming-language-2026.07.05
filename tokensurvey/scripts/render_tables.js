const fs=require('fs');
function readCsv(p){
  const [h,...ls]=fs.readFileSync(p,'utf8').trim().split('\n'); const cols=h.split(',');
  return ls.map(l=>{const v=[];let cur='',q=false;
    for(let i=0;i<l.length;i++){const c=l[i];
      if(q){ if(c==='"'){ if(l[i+1]==='"'){cur+='"';i++;} else q=false; } else cur+=c; }
      else if(c==='"')q=true; else if(c===','){v.push(cur);cur='';} else cur+=c;}
    v.push(cur); const o={}; cols.forEach((c,i)=>o[c]=v[i]); return o;});
}
const n = x => x===''||x==null ? '–' : x;
const L=readCsv('data/languages_summary.csv'), F=readCsv('data/frameworks_summary.csv');
let out='';
out+='### Table 1 — Programming languages, ranked by token efficiency\n\n';
out+='| # | Language | Efficiency index | Tokens per equivalent task | Lines per task | Tokens per code line | Chars per token | Tokens per edit session |\n';
out+='|---:|---|---:|---:|---:|---:|---:|---:|\n';
for (const r of L){
  out+=`| ${n(r.rank)} | ${r.name} | ${n(r.tei)} | ${n(r.tokens_per_task)} | ${n(r.lines_per_task)} | ${n(r.tokens_per_sloc)} | ${n(r.chars_per_token)} | ${r.session?Math.round(r.session).toLocaleString('en-US'):'–'} |\n`;
}
out+='\n### Table 2 — Frameworks, ranked by token efficiency\n\n';
out+='| # | Framework | Efficiency index | Tokens per code line | Host language | vs. host language | Median tokens per file | Tokens per edit session |\n';
out+='|---:|---|---:|---:|---|---:|---:|---:|\n';
for (const r of F){
  out+=`| ${r.rank} | ${r.name} | ${r.tei} | ${r.tokens_per_sloc} | ${r.host_language||'–'} | ${r.vs_host_language?(r.vs_host_language+'×'):'–'} | ${Number(r.med_tok_file).toLocaleString('en-US')} | ${r.session?Math.round(r.session).toLocaleString('en-US'):'–'} |\n`;
}
fs.writeFileSync('data/tables.md',out);
console.log(out);
