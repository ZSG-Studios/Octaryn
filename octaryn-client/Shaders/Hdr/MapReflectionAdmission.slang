uint2 reflection_recovery_admission(uint highCount,uint lowCount,uint queryBudget) {
    // Reserve one possible secondary visibility query for each reflection ray.
    uint high=min(highCount,queryBudget/12u);
    return uint2(high,min(lowCount,(queryBudget-high*12u)/4u));
}
