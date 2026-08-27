// Comment syntax per canonical language name (for SLOC), plus extension fallback.
const L = {
  'Ada':{line:['--']}, 'Apex':{line:['//'],block:[['/*','*/']]},
  'Assembly':{line:[';','#']}, 'Bash/Shell':{line:['#']},
  'C':{line:['//'],block:[['/*','*/']]}, 'C#':{line:['//'],block:[['/*','*/']]},
  'C++':{line:['//'],block:[['/*','*/']]}, 'Clojure':{line:[';']},
  'Cobol':{line:['*>','*','C ']}, 'Crystal':{line:['#']},
  'Dart':{line:['//'],block:[['/*','*/']]}, 'Delphi':{line:['//'],block:[['{','}'],['(*','*)']]},
  'Elixir':{line:['#']}, 'Erlang':{line:['%']}, 'F#':{line:['//'],block:[['(*','*)']]},
  'Fortran':{line:['!','C','c','*']}, 'GDScript':{line:['#']}, 'Gleam':{line:['//']},
  'Go':{line:['//'],block:[['/*','*/']]}, 'Groovy':{line:['//'],block:[['/*','*/']]},
  'HTML/CSS':{block:[['<!--','-->'],['/*','*/']]}, 'Haskell':{line:['--'],block:[['{-','-}']]},
  'Java':{line:['//'],block:[['/*','*/']]}, 'JavaScript':{line:['//'],block:[['/*','*/']]},
  'Julia':{line:['#']}, 'Kotlin':{line:['//'],block:[['/*','*/']]}, 'Lisp':{line:[';']},
  'Lua':{line:['--'],block:[['--[[',']]']]}, 'MATLAB':{line:['%'],block:[['%{','%}']]},
  'MicroPython':{line:['#']}, 'Nim':{line:['#']}, 'OCaml':{block:[['(*','*)']]},
  'Objective-C':{line:['//'],block:[['/*','*/']]}, 'PHP':{line:['//','#'],block:[['/*','*/']]},
  'Perl':{line:['#']}, 'PowerShell':{line:['#'],block:[['<#','#>']]}, 'Prolog':{line:['%'],block:[['/*','*/']]},
  'Python':{line:['#']}, 'R':{line:['#']}, 'Ruby':{line:['#']}, 'Rust':{line:['//'],block:[['/*','*/']]},
  'SQL':{line:['--'],block:[['/*','*/']]}, 'Scala':{line:['//'],block:[['/*','*/']]},
  'Solidity':{line:['//'],block:[['/*','*/']]}, 'Swift':{line:['//'],block:[['/*','*/']]},
  'Tcl':{line:['#']}, 'TypeScript':{line:['//'],block:[['/*','*/']]}, 'VBA':{line:["'"]},
  'Visual Basic (.Net)':{line:["'"]}, 'Zig':{line:['//']},
};
const EXT = {
  js:'JavaScript', jsx:'JavaScript', mjs:'JavaScript', cjs:'JavaScript',
  ts:'TypeScript', tsx:'TypeScript', vue:'HTML/CSS', svelte:'HTML/CSS', razor:'HTML/CSS',
  py:'Python', rb:'Ruby', php:'PHP', module:'PHP', java:'Java', kt:'Kotlin', scala:'Scala',
  go:'Go', rs:'Rust', cs:'C#', c:'C', h:'C', cc:'C++', cpp:'C++', hpp:'C++',
  m:'Objective-C', swift:'Swift', dart:'Dart', ex:'Elixir', exs:'Elixir', erl:'Erlang',
  elm:'Haskell', hs:'Haskell', ml:'OCaml', fs:'F#', clj:'Clojure', lisp:'Lisp',
  lua:'Lua', pl:'Prolog', pm:'Perl', r:'R', R:'R', jl:'Julia', nim:'Nim', cr:'Crystal',
  sh:'Bash/Shell', ps1:'PowerShell', psm1:'PowerShell', sql:'SQL', tcl:'Tcl', zig:'Zig',
  sol:'Solidity', groovy:'Groovy', gd:'GDScript', gleam:'Gleam', vb:'Visual Basic (.Net)',
  cls:'Apex', bas:'VBA', adb:'Ada', ads:'Ada', pas:'Delphi', f:'Fortran', f90:'Fortran',
  cbl:'Cobol', cob:'Cobol', asm:'Assembly', s:'Assembly', html:'HTML/CSS', css:'HTML/CSS',
};
function cfgFor(name, ext){
  if (L[name]) return L[name];
  const byExt = EXT[ext] || EXT[(ext||'').toLowerCase()];
  if (byExt && L[byExt]) return L[byExt];
  return {line:['//','#'],block:[['/*','*/']]};
}
function slocCount(text, cfg){
  const lines = text.split('\n');
  let sloc=0, nonblank=0, comment=0, inBlock=null;
  for (const raw of lines){
    const t = raw.trim();
    if (!t) continue;
    nonblank++;
    if (inBlock){ comment++; if (t.includes(inBlock)) inBlock=null; continue; }
    let isC=false;
    for (const p of (cfg.line||[])) if (t.startsWith(p)) { isC=true; break; }
    if (!isC) for (const [o,c] of (cfg.block||[])){
      if (t.startsWith(o)){ isC=true; if (!t.slice(o.length).includes(c)) inBlock=c; break; }
    }
    if (isC) comment++; else sloc++;
  }
  return {lines: lines.length, nonblank, sloc, comment};
}
// Split into code-only text (comment-only and blank lines removed) and comment text.
function splitCode(text, cfg){
  const lines = text.split('\n');
  const code=[], comm=[]; let inBlock=null;
  for (const raw of lines){
    const t = raw.trim();
    if (!t) continue;
    if (inBlock){ comm.push(raw); if (t.includes(inBlock)) inBlock=null; continue; }
    let isC=false;
    for (const p of (cfg.line||[])) if (t.startsWith(p)) { isC=true; break; }
    if (!isC) for (const [o,c] of (cfg.block||[])){
      if (t.startsWith(o)){ isC=true; if (!t.slice(o.length).includes(c)) inBlock=c; break; }
    }
    (isC?comm:code).push(raw);
  }
  return {code: code.join('\n'), comments: comm.join('\n')};
}
module.exports = {cfgFor, slocCount, splitCode};
