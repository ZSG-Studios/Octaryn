uint visible_face_mask(uint currentSolid, uint neighborSolid)
{
    return currentSolid & ~neighborSolid;
}
