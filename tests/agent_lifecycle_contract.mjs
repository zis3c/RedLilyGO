import fs from 'node:fs';
import assert from 'node:assert/strict';

const agent = fs.readFileSync('src/Attacks/Agent/Agent.cpp', 'utf8');
const program = fs.readFileSync('tools/Agent/Program.cs', 'utf8');
const serial = fs.readFileSync('tools/Agent/SerialComms.cs', 'utf8');
const transport = fs.readFileSync('tools/Agent/TransportStream.cs', 'utf8');
const executer = fs.readFileSync('tools/Agent/Executer.cs', 'utf8');

assert.match(agent, /Agent heartbeat timed out/);
assert.match(agent, /USB mode changed/);
assert.match(agent, /m_protocolCompatible = false/);
assert.match(program, /catch \(OperationCanceledException\) when \(lifetime\.IsCancellationRequested\)/);
assert.match(program, /serial\.Start\(vid, pid, lifetime\.Token, Log\)/);
assert.match(program, /--help/);
assert.match(program, /--version/);
assert.match(program, /Usage: RedLilyGO\.Agent\.exe/);
assert.match(program, /protocol \{AgentInfo\.ProtocolVersion\}/);
assert.match(serial, /status\?\.Invoke\(\$"Board connected on/);
assert.match(transport, /Remote transport closed while reading/);
assert.match(transport, /MaxPendingWriteBytes/);
assert.match(transport, /blocksToWrite\.Add\(chunk, cts\.Token\)/);
assert.match(transport, /DataToStreamOwner\.SetLength\(0\)/);
assert.match(executer, /catch \(OperationCanceledException\)/);
assert.match(executer, /late packet from a remote session/);
assert.match(executer, /CommandTimeout/);
assert.match(executer, /commandGate/);
assert.match(executer, /MaxRemoteQueueItems/);
assert.match(executer, /outputGate/);

console.log('AGENT_LIFECYCLE_CONTRACT: pass');
