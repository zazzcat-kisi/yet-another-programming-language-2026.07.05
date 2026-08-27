const fs=require('fs');
function rd(p){const [h,...l]=fs.readFileSync(p,'utf8').trim().split('\n');const c=h.split(',');
 return l.map(x=>{const v=[];let cur='',q=false;for(let i=0;i<x.length;i++){const ch=x[i];
  if(q){if(ch==='"'){if(x[i+1]==='"'){cur+='"';i++;}else q=false;}else cur+=ch;}
  else if(ch==='"')q=true;else if(ch===','){v.push(cur);cur='';}else cur+=ch;}v.push(cur);
  const o={};c.forEach((k,i)=>o[k]=v[i]);return o;});}
const L=rd('data/languages_summary.csv').map(r=>({
  rank:r.rank?+r.rank:null, name:r.name, tei:r.tei?+r.tei:null,
  tpt:r.tokens_per_task?+r.tokens_per_task:null, lpt:r.lines_per_task?+r.lines_per_task:null,
  tps:+r.tokens_per_sloc, cpt:+r.chars_per_token, ses:r.session?Math.round(+r.session):null,
  tasks:+r.rosetta_tasks||null, repo:r.repos, files:+r.files, mtf:Math.round(+r.med_tok_file)
}));
const F=rd('data/frameworks_summary.csv').map(r=>({
  rank:+r.rank, name:r.name, tei:+r.tei, tps:+r.tokens_per_sloc, host:r.host_language||null,
  vs:r.vs_host_language?+r.vs_host_language:null, mtf:Math.round(+r.med_tok_file),
  ses:r.session?Math.round(+r.session):null, cpt:+r.chars_per_token, repo:r.repos, files:+r.files
}));
const tpl=fs.readFileSync('scripts/artifact_template.html','utf8');
const out=tpl.replace('/*__LANGS__*/','const LANGS='+JSON.stringify(L)+';')
             .replace('/*__FWS__*/','const FWS='+JSON.stringify(F)+';');
fs.writeFileSync(process.argv[2],out);
console.log('wrote',process.argv[2],(out.length/1024).toFixed(1)+'KB');
