import fs from 'node:fs';
import path from 'node:path';

// The web service is split across several translation units. Concatenate all of
// them so source contracts keep matching regardless of which file a symbol
// lives in after future refactors.
const webDir = path.resolve('src/Comms/Web');

export const webSource = fs
  .readdirSync(webDir)
  .filter((name) => name.endsWith('.cpp'))
  .sort()
  .map((name) => fs.readFileSync(path.join(webDir, name), 'utf8'))
  .join('\n');
