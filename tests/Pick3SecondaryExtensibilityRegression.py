from pathlib import Path
s = (Path(__file__).resolve().parents[1] / "inc/pick3/PickFromThreeSubmission.hpp").read_text()
assert "enum class Pick3SecondaryDecision" in s
assert "RegisterPick3SecondaryFilterEvaluator" in s
assert '"SECONDARY_EVALUATOR_REQUIRED"' in s
assert 'Pick3SecondaryDecision::Allow' in s
assert "pick3PhysicalPairCheck(pick3CurrentValidationModel" in s
assert "first.effectId != pick3PrimaryCaptureEffect" in s
print("PASS secondary evaluator extension seam")
