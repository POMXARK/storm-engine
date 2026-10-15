technique AimZone
{
    pass p0
    {
        // on the water and behind the hulls: tested against the depth, not written into it
        ZEnable = true;
        ZWriteEnable = false;
        Lighting = false;
        CullMode = none;
        SpecularEnable = false;
        AlphaTestEnable = false;
        AlphaBlendEnable = true;
        SrcBlend = srcalpha;
        DestBlend = invsrcalpha;

        ColorOp[0] = selectarg1;
        ColorArg1[0] = diffuse;

        AlphaOp[0] = selectarg1;
        AlphaArg1[0] = diffuse;

        ColorOp[1] = disable;
    }
}
