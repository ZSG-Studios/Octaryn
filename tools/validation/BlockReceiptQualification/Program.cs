using System.Text.Json;
using Octaryn.Server.World.Blocks;
using Octaryn.Shared.Host;
using Octaryn.Shared.World;

var root = Path.GetFullPath(args.Length == 1 ? args[0] : "logs/tools/block-receipt-qualification");
Directory.CreateDirectory(root);
var cells = new Dictionary<BlockPosition, BlockId>();
ulong revision = 7;
var position = new BlockPosition(31, 4, -1);
cells[position] = new(9);
var ledger = new BlockReceiptLedger(root, p => cells.GetValueOrDefault(p), () => revision);
var json = new JsonSerializerOptions { PropertyNamingPolicy = JsonNamingPolicy.CamelCase };
BlockReceiptBatch Read() => JsonSerializer.Deserialize<BlockReceiptBatch>(File.ReadAllText(Path.Combine(root,"block_results.json")), json)!;
void Require(bool value, string label) { if (!value) throw new InvalidOperationException(label); }
HostCommand Command(ulong id) => new() { RequestId=id, A=position.X, B=position.Y, C=position.Z };

ledger.SaveAndPublish(() => { }, revision);
Require(!ledger.TryReserveAfterMovement(1, position, 10, 9) && ledger.Count == 0, "defer until authoritative movement consumed");
Require(ledger.TryReserveAfterMovement(1, position, 10, 10), "reserve after authoritative movement");
ledger.Reject(Command(1));
Require(Read().Receipts.Count == 0, "staged results not published prematurely");
ledger.SaveAndPublish(() => { }, revision);
var rejected = Read().Receipts.Single();
Require(!rejected.Accepted && rejected.Revision == 7 && rejected.Blocks.Single().Block == 9, "unchanged revision reject with authoritative cell");
Require(!ledger.Acknowledge(new(1, "wrong", 1)), "wrong session ack");
Require(!ledger.Acknowledge(new(1, ledger.Session, 2)), "future ack");

Require(ledger.TryReserve(2, position), "reserve accept");
cells[position] = new(0);revision++;
ledger.Record(Command(2), new(true, true, [new(position, new(0))]));
try { ledger.SaveAndPublish(() => throw new IOException("injected save failure"), revision); }
catch (IOException) { }
Require(Read().Receipts.Count == 1 && ledger.Count == 2, "failed save cannot publish acceptance or free capacity");
bool saved = false;
ledger.SaveAndPublish(() => saved=true, revision);
var batch=Read();
Require(saved && batch.Receipts.Count==2 && batch.Receipts[1].Accepted && batch.Receipts[1].Revision==8, "save before accepted receipt");
Require(batch.Receipts[1].Sequence==2 && batch.Receipts[1].Blocks.Single().Block==0, "ordered authoritative post-command value");
Require(ledger.Acknowledge(new(1,ledger.Session,2)) && ledger.Count==0, "ack releases capacity");
Require(!ledger.Acknowledge(new(1,ledger.Session,1)), "regressing ack");
Require(!ledger.TryReserve(2,position), "acknowledged command cannot replay");
for(ulong id=3;id<3+BlockReceiptLedger.Capacity;++id) Require(ledger.TryReserve(id,position), "fill reservations");
Require(!ledger.TryReserve(1000,position), "reservation backpressure");
var replacement=new BlockReceiptLedger(root,p=>cells.GetValueOrDefault(p),()=>revision);
Require(replacement.Session!=ledger.Session && replacement.Count==0 && cells[position].Value==0, "reconnect preserves durable world");
replacement.SaveAndPublish(()=>{},revision);
Require(Read().Session==replacement.Session && Read().Receipts.Count==0, "new session replaces stale mailbox");
Console.WriteLine("block_receipt_qualification PASS ordered_results unchanged_revision_reject durable_barrier ack_validation bounded_capacity reconnect");
