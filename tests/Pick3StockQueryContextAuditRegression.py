from pathlib import Path

source = Path(__file__).resolve().parents[1] / "inc/pick3/PickFromThreeSubmission.hpp"
s = source.read_text()
assert "PICK3_STOCK_QUERY_CONTEXT" in s
assert 'AuditPick3StockQueryContext("CONVERTER_ENTER"' in s
assert 'AuditPick3StockQueryContext("SECONDARY_ENTER"' in s
assert "pick3StockContextAuditSamples >= 96" in s
assert "if (!effectContext)" in s
print("Pick3 stock query-context audit source checks passed")
