// Maintainer-only compiler-library fixture export, never a public language API.
import fs from 'node:fs';
import {createRequire} from 'node:module';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
const args=process.argv.slice(2);const check=args[0]==='--check';if(check)args.shift();
const [input,output]=args;const source=path.resolve(input);
try {
 const module=process.env.LMCODE_COMPILER_MODULE ?? path.join(path.dirname(createRequire(pathToFileURL(fs.realpathSync(process.env.LMSCRIPT_BIN))).resolve('@lmctl-ai/lmctl/package.json')),'dist/dsl.js');
 const {CompiledDsl}=await import(pathToFileURL(module).href);
 const {LMScript}=await import(pathToFileURL(path.join(path.dirname(module),'lmscript.js')).href);
 const engine=new LMScript();let bytes;
 engine.registerFunction('fixture_capture',(p,target)=>{bytes=new Uint8Array(CompiledDsl.bytesOf(p));if(target)fs.writeFileSync(target,bytes);return null;},2,{isDefault:true});
 const raw=fs.readFileSync(source,'utf8');
 const result=await engine.runAsync(raw+(output?'\nfixture_capture(program, null);':''),{file:source,platform:{cwd:path.dirname(source),env:process.env}});
 if(!result.success){for(const d of result.diagnostics)console.error(`${source}:${d.line}:${d.column}: ${d.message}`);process.exitCode=result.exitCode??2;}
 else if(!check){if(!bytes)throw Error('fixture did not construct program');if(output)fs.writeFileSync(output,bytes);}
}catch(e){console.error(`${source}:1:1: ${e.message}`);process.exitCode=2;}
