// Flags files that are data tables, generated boilerplate or vendored blobs rather than
// hand-written code. Such files distort tokens-per-line badly.
const fs=require('fs'), path=require('path');
const {encode}=require('gpt-tokenizer/encoding/o200k_base');
const {cfgFor, splitCode}=require('./langmeta.js');
const MARKUP=new Set(['html','css','scss','vue','svelte','razor','astro','xml','md','json','yaml','yml']);
function fileStats(p, text){
  const ext=(p.split('.').pop()||'').toLowerCase();
  const lines=text.split('\n').filter(l=>l.trim());
  let dataLines=0;
  for (const l of lines){
    const t=l.trim();
    const letters=(t.match(/[A-Za-z]/g)||[]).length;
    if (letters/t.length < 0.35) dataLines++;
  }
  const nonascii=(text.match(/[^\x00-\x7F]/g)||[]).length/Math.max(1,text.length);
  const code=splitCode(text, cfgFor('', ext)).code;
  const sloc=code.split('\n').filter(l=>l.trim()).length;
  const tpl= sloc ? encode(code).length/sloc : 0;
  return {dataShare: lines.length? dataLines/lines.length : 0, nonascii, tokensPerCodeLine: tpl, markup: MARKUP.has(ext)};
}
function isDataFile(p, text){
  const s=fileStats(p, text);
  return s.dataShare > 0.6 || s.nonascii > 0.10 || (!s.markup && s.tokensPerCodeLine > 20);
}
module.exports={fileStats,isDataFile};
if (require.main===module){
  const p=process.argv[2];
  const t=fs.readFileSync(p,'utf8');
  const s=fileStats(p,t);
  console.log(p, JSON.stringify({d:+s.dataShare.toFixed(2),n:+s.nonascii.toFixed(3),t:+s.tokensPerCodeLine.toFixed(1)}), isDataFile(p,t)?'DATA':'code');
}
