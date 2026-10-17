// 0x007B0D50
std::uint32_t moveOntoWallMachineCtor(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    eax = stk[sp + 2];
    stk[--sp] = esi;
    stk[--sp] = 0u;
    stk[--sp] = 1u;
    esi = ecx;
    ecx = stk[sp + 4];
    stk[--sp] = eax;
    stk[--sp] = ecx;
    ecx = esi;
    { const std::uint32_t a[4] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3]}; eax = callExternal(env, 0x005B6150u, ecx, a, 4); }
    sp += 4;
    stk[--sp] = 88u;
    store32(asPtr(esi), 0, 12764160u);
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    sp += 1;
    if (eax == 0) goto L_007B0D86;
    stk[--sp] = esi;
    ecx = eax;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x007A59D0u, ecx, a, 1); }
    sp += 1;
    goto L_007B0D88;
L_007B0D86:
    eax = 0;
L_007B0D88:
    stk[--sp] = edi;
    stk[--sp] = 0u;
    stk[--sp] = 9999u;
    stk[--sp] = 1u;
    stk[--sp] = eax;
    stk[--sp] = 0u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 40u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == 0) goto L_007B0DCC;
    stk[--sp] = 3324162312u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x20, 0u);
    asPtr(edi)[0x24] = static_cast<std::uint8_t>(0);
    store32(asPtr(edi), 0, 12769512u);
    goto L_007B0DCE;
L_007B0DCC:
    edi = 0;
L_007B0DCE:
    stk[--sp] = 0u;
    stk[--sp] = 9999u;
    stk[--sp] = 2u;
    stk[--sp] = edi;
    stk[--sp] = 1u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 84u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    sp += 1;
    edi = stk[sp++];
    if (eax == 0) goto L_007B0DFA;
    stk[--sp] = esi;
    ecx = eax;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x007A22C0u, ecx, a, 1); }
    sp += 1;
    goto L_007B0DFC;
L_007B0DFA:
    eax = 0;
L_007B0DFC:
    stk[--sp] = 0u;
    stk[--sp] = 9998u;
    stk[--sp] = 9998u;
    stk[--sp] = eax;
    stk[--sp] = 2u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    eax = esi;
    esi = stk[sp++];
    return eax;
}
// 0x007B6840
std::uint32_t squadCtor(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    eax = ecx;
    ecx = 0;
    store32(asPtr(eax), 0, 12788504u);
    asPtr(eax)[4] = static_cast<std::uint8_t>((ecx & 0xFFu));
    store32(asPtr(eax), 8, ecx);
    store32(asPtr(eax), 0xc, ecx);
    store32(asPtr(eax), 0x10, ecx);
    asPtr(eax)[0x18] = static_cast<std::uint8_t>((ecx & 0xFFu));
    store32(asPtr(eax), 0x1c, ecx);
    store32(asPtr(eax), 0x20, ecx);
    store32(asPtr(eax), 0x24, ecx);
    asPtr(eax)[0x2c] = static_cast<std::uint8_t>((ecx & 0xFFu));
    return eax;
}
// 0x007B9710
std::uint32_t aiStateMachine30530Slot18(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    stk[--sp] = ebx;
    stk[--sp] = esi;
    stk[--sp] = edi;
    esi = ecx;
    eax = call0(env, 0x007B8410u, ecx);
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    ebx = 0;
    sp += 1;
    if (edi == ebx) goto L_007B9741;
    stk[--sp] = 3643917292u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0, 12760224u);
    goto L_007B9743;
L_007B9741:
    edi = 0;
L_007B9743:
    stk[--sp] = 12790400u;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDC20u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == ebx) goto L_007B977B;
    stk[--sp] = 3017849609u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x20, ebx);
    store32(asPtr(edi), 0, 12775888u);
    goto L_007B977D;
L_007B977B:
    edi = 0;
L_007B977D:
    stk[--sp] = ebx;
    stk[--sp] = 9999u;
    stk[--sp] = 9998u;
    stk[--sp] = edi;
    stk[--sp] = 54u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDC20u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == ebx) goto L_007B97BA;
    stk[--sp] = 1275970061u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x20, ebx);
    store32(asPtr(edi), 0, 12772304u);
    goto L_007B97BC;
L_007B97BA:
    edi = 0;
L_007B97BC:
    stk[--sp] = 12790368u;
    stk[--sp] = 54u;
    stk[--sp] = 54u;
    stk[--sp] = edi;
    stk[--sp] = 9u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDC20u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 44u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == ebx) goto L_007B9801;
    stk[--sp] = 1128509345u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x20, ebx);
    store32(asPtr(edi), 0, 12772200u);
    store32(asPtr(edi), 0x24, 1u);
    store32(asPtr(edi), 0x28, ebx);
    goto L_007B9803;
L_007B9801:
    edi = 0;
L_007B9803:
    stk[--sp] = 12790368u;
    stk[--sp] = 54u;
    stk[--sp] = 54u;
    stk[--sp] = edi;
    stk[--sp] = 10u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDC20u, ecx, a, 5); }
    sp += 5;
    edi = stk[sp++];
    esi = stk[sp++];
    ebx = stk[sp++];
    return eax;
}
// 0x007B8E70
std::uint32_t aiStateMachine30480Slot18(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    stk[--sp] = ebx;
    stk[--sp] = esi;
    stk[--sp] = edi;
    esi = ecx;
    eax = call0(env, 0x007B8A40u, ecx);
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    ebx = 0;
    sp += 1;
    if (edi == ebx) goto L_007B8EA1;
    stk[--sp] = 3643917292u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0, 12760224u);
    goto L_007B8EA3;
L_007B8EA1:
    edi = 0;
L_007B8EA3:
    stk[--sp] = 12752984u;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDC20u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 44u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == ebx) goto L_007B8EE5;
    stk[--sp] = 1128509345u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x20, ebx);
    store32(asPtr(edi), 0, 12772200u);
    store32(asPtr(edi), 0x24, 1u);
    store32(asPtr(edi), 0x28, ebx);
    goto L_007B8EE7;
L_007B8EE5:
    edi = 0;
L_007B8EE7:
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = edi;
    stk[--sp] = 10u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == ebx) goto L_007B8F1C;
    stk[--sp] = 1275970061u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x20, ebx);
    store32(asPtr(edi), 0, 12772304u);
    goto L_007B8F1E;
L_007B8F1C:
    edi = 0;
L_007B8F1E:
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = edi;
    stk[--sp] = 9u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 44u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == ebx) goto L_007B8F5D;
    stk[--sp] = 1128509345u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x20, ebx);
    store32(asPtr(edi), 0x24, 1u);
    store32(asPtr(edi), 0x28, ebx);
    store32(asPtr(edi), 0, 12773816u);
    goto L_007B8F5F;
L_007B8F5D:
    edi = 0;
L_007B8F5F:
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = edi;
    stk[--sp] = 11u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == ebx) goto L_007B8F94;
    stk[--sp] = 1002916402u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x20, ebx);
    store32(asPtr(edi), 0, 12771744u);
    goto L_007B8F96;
L_007B8F94:
    edi = 0;
L_007B8F96:
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = edi;
    stk[--sp] = 93u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == ebx) goto L_007B8FCB;
    stk[--sp] = 1664555736u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x20, ebx);
    store32(asPtr(edi), 0, 12774136u);
    goto L_007B8FCD;
L_007B8FCB:
    edi = 0;
L_007B8FCD:
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = edi;
    stk[--sp] = 16u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == ebx) goto L_007B9002;
    stk[--sp] = 424136815u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x20, ebx);
    store32(asPtr(edi), 0, 12774032u);
    goto L_007B9004;
L_007B9002:
    edi = 0;
L_007B9004:
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = edi;
    stk[--sp] = 79u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == ebx) goto L_007B9039;
    stk[--sp] = 869595447u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x20, ebx);
    store32(asPtr(edi), 0, 12774248u);
    goto L_007B903B;
L_007B9039:
    edi = 0;
L_007B903B:
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = edi;
    stk[--sp] = 94u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == ebx) goto L_007B906D;
    stk[--sp] = 2029093128u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0, 12759800u);
    goto L_007B906F;
L_007B906D:
    edi = 0;
L_007B906F:
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = edi;
    stk[--sp] = 95u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == ebx) goto L_007B90A4;
    stk[--sp] = 658954461u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x20, ebx);
    store32(asPtr(edi), 0, 12772088u);
    goto L_007B90A6;
L_007B90A4:
    edi = 0;
L_007B90A6:
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = edi;
    stk[--sp] = 33u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDC20u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == ebx) goto L_007B90DB;
    stk[--sp] = 285302123u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x20, ebx);
    store32(asPtr(edi), 0, 12783560u);
    goto L_007B90DD;
L_007B90DB:
    edi = 0;
L_007B90DD:
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = edi;
    stk[--sp] = 35u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == ebx) goto L_007B9121;
    stk[--sp] = 285302123u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = edi;
    stk[--sp] = 34u;
    ecx = esi;
    store32(asPtr(edi), 0x20, ebx);
    store32(asPtr(edi), 0, 12783560u);
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    edi = stk[sp++];
    esi = stk[sp++];
    ebx = stk[sp++];
    return eax;
L_007B9121:
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    edi = 0;
    stk[--sp] = edi;
    stk[--sp] = 34u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    edi = stk[sp++];
    esi = stk[sp++];
    ebx = stk[sp++];
    return eax;
}
// 0x007B98D0
std::uint32_t aiStateMachine30638Slot18(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    stk[--sp] = ebx;
    stk[--sp] = esi;
    stk[--sp] = edi;
    stk[--sp] = 32u;
    esi = ecx;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    edi = 0;
    sp += 1;
    if (ebx == edi) goto L_007B98FC;
    stk[--sp] = 3643917292u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0, 12760224u);
    goto L_007B98FE;
L_007B98FC:
    ebx = 0;
L_007B98FE:
    stk[--sp] = 12752984u;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9933;
    stk[--sp] = 102591889u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0, 12757736u);
    goto L_007B9935;
L_007B9933:
    ebx = 0;
L_007B9935:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 42u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9967;
    stk[--sp] = 3743118515u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0, 12758240u);
    goto L_007B9969;
L_007B9967:
    ebx = 0;
L_007B9969:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 13u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B999E;
    stk[--sp] = 1678529186u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12775232u);
    goto L_007B99A0;
L_007B999E:
    ebx = 0;
L_007B99A0:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 1u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B99D5;
    stk[--sp] = 1019052890u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12774792u);
    goto L_007B99D7;
L_007B99D5:
    ebx = 0;
L_007B99D7:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 26u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 40u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9A13;
    stk[--sp] = 29571194u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12784144u);
    store32(asPtr(ebx), 0x24, 4294967295u);
    goto L_007B9A15;
L_007B9A13:
    ebx = 0;
L_007B9A15:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 7u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9A4A;
    stk[--sp] = 658954461u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12772088u);
    goto L_007B9A4C;
L_007B9A4A:
    ebx = 0;
L_007B9A4C:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 33u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9A81;
    stk[--sp] = 29571194u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12773352u);
    goto L_007B9A83;
L_007B9A81:
    ebx = 0;
L_007B9A83:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 6u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9AB8;
    stk[--sp] = 29571194u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12773352u);
    goto L_007B9ABA;
L_007B9AB8:
    ebx = 0;
L_007B9ABA:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 52u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9AEF;
    stk[--sp] = 285302123u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12773576u);
    goto L_007B9AF1;
L_007B9AEF:
    ebx = 0;
L_007B9AF1:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 2u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9B26;
    stk[--sp] = 285302123u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12773456u);
    goto L_007B9B28;
L_007B9B26:
    ebx = 0;
L_007B9B28:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 3u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9B5D;
    stk[--sp] = 29571194u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12783440u);
    goto L_007B9B5F;
L_007B9B5D:
    ebx = 0;
L_007B9B5F:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 59u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9B94;
    stk[--sp] = 285302123u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12783560u);
    goto L_007B9B96;
L_007B9B94:
    ebx = 0;
L_007B9B96:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 35u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9BCB;
    stk[--sp] = 285302123u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12783560u);
    goto L_007B9BCD;
L_007B9BCB:
    ebx = 0;
L_007B9BCD:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 34u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9C02;
    stk[--sp] = 285302123u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12773696u);
    goto L_007B9C04;
L_007B9C02:
    ebx = 0;
L_007B9C04:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 4u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9C39;
    stk[--sp] = 285302123u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12773696u);
    goto L_007B9C3B;
L_007B9C39:
    ebx = 0;
L_007B9C3B:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 5u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9C6D;
    stk[--sp] = 1777983297u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0, 12760840u);
    goto L_007B9C6F;
L_007B9C6D:
    ebx = 0;
L_007B9C6F:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 37u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9CA1;
    stk[--sp] = 1004728148u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0, 12759152u);
    goto L_007B9CA3;
L_007B9CA1:
    ebx = 0;
L_007B9CA3:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = 78u;
    stk[--sp] = ebx;
    stk[--sp] = 88u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9CD9;
    stk[--sp] = 651740892u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12775128u);
    goto L_007B9CDB;
L_007B9CD9:
    ebx = 0;
L_007B9CDB:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = 90u;
    stk[--sp] = ebx;
    stk[--sp] = 78u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9D0E;
    stk[--sp] = 3643917292u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0, 12760320u);
    goto L_007B9D10;
L_007B9D0E:
    ebx = 0;
L_007B9D10:
    stk[--sp] = 12752984u;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 90u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 44u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9D53;
    stk[--sp] = 1128509345u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12772200u);
    store32(asPtr(ebx), 0x24, 1u);
    store32(asPtr(ebx), 0x28, edi);
    goto L_007B9D55;
L_007B9D53:
    ebx = 0;
L_007B9D55:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 10u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9D8A;
    stk[--sp] = 1275970061u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12772304u);
    goto L_007B9D8C;
L_007B9D8A:
    ebx = 0;
L_007B9D8C:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 9u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 44u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9DCB;
    stk[--sp] = 1128509345u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0x24, 1u);
    store32(asPtr(ebx), 0x28, edi);
    store32(asPtr(ebx), 0, 12773816u);
    goto L_007B9DCD;
L_007B9DCB:
    ebx = 0;
L_007B9DCD:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 11u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9E02;
    stk[--sp] = 998214717u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12772512u);
    goto L_007B9E04;
L_007B9E02:
    ebx = 0;
L_007B9E04:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 22u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9E39;
    stk[--sp] = 1149065088u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0, 12773040u);
    store32(asPtr(ebx), 0x20, edi);
    goto L_007B9E3B;
L_007B9E39:
    ebx = 0;
L_007B9E3B:
    stk[--sp] = 12790496u;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 96u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9E74;
    stk[--sp] = 1002916402u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12771744u);
    goto L_007B9E76;
L_007B9E74:
    ebx = 0;
L_007B9E76:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 93u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9EAB;
    stk[--sp] = 1664555736u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12774136u);
    goto L_007B9EAD;
L_007B9EAB:
    ebx = 0;
L_007B9EAD:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 16u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9EE2;
    stk[--sp] = 424136815u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12774032u);
    goto L_007B9EE4;
L_007B9EE2:
    ebx = 0;
L_007B9EE4:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 79u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9F19;
    stk[--sp] = 869595447u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12774248u);
    goto L_007B9F1B;
L_007B9F19:
    ebx = 0;
L_007B9F1B:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 94u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9F4D;
    stk[--sp] = 2029093128u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0, 12759800u);
    goto L_007B9F4F;
L_007B9F4D:
    ebx = 0;
L_007B9F4F:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 95u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9F84;
    stk[--sp] = 2764187662u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12773144u);
    goto L_007B9F86;
L_007B9F84:
    ebx = 0;
L_007B9F86:
    stk[--sp] = 12790464u;
    stk[--sp] = edi;
    stk[--sp] = 77u;
    stk[--sp] = ebx;
    stk[--sp] = 15u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007B9FC0;
    stk[--sp] = 588776306u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12775336u);
    goto L_007B9FC2;
L_007B9FC0:
    ebx = 0;
L_007B9FC2:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = 77u;
    stk[--sp] = ebx;
    stk[--sp] = 54u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 84u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    sp += 1;
    if (eax == edi) goto L_007B9FE8;
    stk[--sp] = esi;
    ecx = eax;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x007AC2E0u, ecx, a, 1); }
    sp += 1;
    goto L_007B9FEA;
L_007B9FE8:
    eax = 0;
L_007B9FEA:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = eax;
    stk[--sp] = 55u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007BA01F;
    stk[--sp] = 214954036u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12774904u);
    goto L_007BA021;
L_007BA01F:
    ebx = 0;
L_007BA021:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 30u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007BA056;
    stk[--sp] = 406563380u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12773928u);
    goto L_007BA058;
L_007BA056:
    ebx = 0;
L_007BA058:
    stk[--sp] = 12790432u;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 77u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007BA091;
    stk[--sp] = 842672676u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12772616u);
    goto L_007BA093;
L_007BA091:
    ebx = 0;
L_007BA093:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 81u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007BA0C8;
    stk[--sp] = 3139185244u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12775016u);
    goto L_007BA0CA;
L_007BA0C8:
    ebx = 0;
L_007BA0CA:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 28u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007BA0FF;
    stk[--sp] = 3012095055u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12775448u);
    goto L_007BA101;
L_007BA0FF:
    ebx = 0;
L_007BA101:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 107u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007BA136;
    stk[--sp] = 1870571036u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12775776u);
    goto L_007BA138;
L_007BA136:
    ebx = 0;
L_007BA138:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 68u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    ebx = eax;
    sp += 1;
    if (ebx == edi) goto L_007BA17C;
    stk[--sp] = 1565170883u;
    stk[--sp] = esi;
    ecx = ebx;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = ebx;
    stk[--sp] = 38u;
    ecx = esi;
    store32(asPtr(ebx), 0x20, edi);
    store32(asPtr(ebx), 0, 12773248u);
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    edi = stk[sp++];
    esi = stk[sp++];
    ebx = stk[sp++];
    return eax;
L_007BA17C:
    stk[--sp] = edi;
    stk[--sp] = edi;
    stk[--sp] = edi;
    ebx = 0;
    stk[--sp] = ebx;
    stk[--sp] = 38u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    edi = stk[sp++];
    esi = stk[sp++];
    ebx = stk[sp++];
    return eax;
}
// 0x007BA190
std::uint32_t disguiseAggressiveMachineCtor(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    eax = stk[sp + 1];
    stk[--sp] = esi;
    stk[--sp] = edi;
    stk[--sp] = eax;
    edi = ecx;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x007AA720u, ecx, a, 1); }
    sp += 1;
    stk[--sp] = 36u;
    store32(asPtr(edi), 0, 12790560u);
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == 0) goto L_007BA1D0;
    stk[--sp] = 3973465763u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x20, 0u);
    store32(asPtr(esi), 0, 12772928u);
    goto L_007BA1D2;
L_007BA1D0:
    esi = 0;
L_007BA1D2:
    stk[--sp] = 12790528u;
    stk[--sp] = 0u;
    stk[--sp] = 0u;
    stk[--sp] = esi;
    stk[--sp] = 0u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDC20u, ecx, a, 5); }
    sp += 5;
    eax = edi;
    edi = stk[sp++];
    esi = stk[sp++];
    return eax;
}
// 0x007BA260
std::uint32_t heliEvacuateMachineCtor(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    eax = stk[sp + 1];
    stk[--sp] = esi;
    stk[--sp] = edi;
    stk[--sp] = 0u;
    stk[--sp] = 1u;
    stk[--sp] = 2840649806u;
    stk[--sp] = eax;
    edi = ecx;
    { const std::uint32_t a[4] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3]}; eax = callExternal(env, 0x005B6150u, ecx, a, 4); }
    sp += 4;
    stk[--sp] = 64u;
    store32(asPtr(edi), 0, 12790664u);
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == 0) goto L_007BA2CE;
    stk[--sp] = 102591889u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x38, 0u);
    store32(asPtr(esi), 0x20, 0);
    store32(asPtr(esi), 0x24, 0);
    store32(asPtr(esi), 0x28, 0);
    store32(asPtr(esi), 0x2c, 0);
    store32(asPtr(esi), 0x30, 0);
    store32(asPtr(esi), 0x34, 0);
    store32(asPtr(esi), 0, 12782448u);
    asPtr(esi)[0x3c] = static_cast<std::uint8_t>(0);
    goto L_007BA2D0;
L_007BA2CE:
    esi = 0;
L_007BA2D0:
    stk[--sp] = 0u;
    stk[--sp] = 9999u;
    stk[--sp] = 108u;
    stk[--sp] = esi;
    stk[--sp] = 2000u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == 0) goto L_007BA30B;
    stk[--sp] = 4003663215u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0, 12759048u);
    goto L_007BA30D;
L_007BA30B:
    esi = 0;
L_007BA30D:
    stk[--sp] = 0u;
    stk[--sp] = 1002u;
    stk[--sp] = 1002u;
    stk[--sp] = esi;
    stk[--sp] = 108u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 60u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == 0) goto L_007BA370;
    stk[--sp] = 102591889u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x38, 0u);
    store32(asPtr(esi), 0x20, 0);
    store32(asPtr(esi), 0x24, 0);
    store32(asPtr(esi), 0x28, 0);
    store32(asPtr(esi), 0x2c, 0);
    store32(asPtr(esi), 0x30, 0);
    store32(asPtr(esi), 0x34, 0);
    store32(asPtr(esi), 0, 12782544u);
    goto L_007BA372;
L_007BA370:
    esi = 0;
L_007BA372:
    stk[--sp] = 0u;
    stk[--sp] = 9999u;
    stk[--sp] = 9998u;
    stk[--sp] = esi;
    stk[--sp] = 1002u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    eax = edi;
    edi = stk[sp++];
    esi = stk[sp++];
    return eax;
}
// 0x007BA7F0
std::uint32_t jetAttackMoveFollowPathMachineCtor(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    eax = stk[sp + 1];
    stk[--sp] = ebx;
    stk[--sp] = esi;
    stk[--sp] = edi;
    stk[--sp] = eax;
    edi = ecx;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x007AB5B0u, ecx, a, 1); }
    sp += 1;
    stk[--sp] = 60u;
    store32(asPtr(edi), 0, 12791088u);
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    ebx = 0;
    sp += 1;
    if (esi == ebx) goto L_007BA850;
    stk[--sp] = 102591889u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x38, ebx);
    store32(asPtr(esi), 0x20, 0);
    store32(asPtr(esi), 0x24, 0);
    store32(asPtr(esi), 0x28, 0);
    store32(asPtr(esi), 0x2c, 0);
    store32(asPtr(esi), 0x30, 0);
    store32(asPtr(esi), 0x34, 0);
    store32(asPtr(esi), 0, 12782544u);
    goto L_007BA852;
L_007BA850:
    esi = 0;
L_007BA852:
    stk[--sp] = ebx;
    stk[--sp] = 9999u;
    stk[--sp] = 104u;
    stk[--sp] = esi;
    stk[--sp] = 1002u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == ebx) goto L_007BA88C;
    stk[--sp] = 1191984986u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0, 12757632u);
    goto L_007BA88E;
L_007BA88C:
    esi = 0;
L_007BA88E:
    stk[--sp] = ebx;
    stk[--sp] = 86u;
    stk[--sp] = 9998u;
    stk[--sp] = esi;
    stk[--sp] = 104u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == ebx) goto L_007BA8CC;
    stk[--sp] = 1312888994u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0, 12774688u);
    store32(asPtr(esi), 0x20, 1u);
    goto L_007BA8CE;
L_007BA8CC:
    esi = 0;
L_007BA8CE:
    stk[--sp] = 12791036u;
    stk[--sp] = 9999u;
    stk[--sp] = 84u;
    stk[--sp] = esi;
    stk[--sp] = 86u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDC20u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 44u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == ebx) goto L_007BA912;
    stk[--sp] = 1128509345u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x20, ebx);
    store32(asPtr(esi), 0, 12772200u);
    store32(asPtr(esi), 0x24, ebx);
    store32(asPtr(esi), 0x28, ebx);
    goto L_007BA914;
L_007BA912:
    esi = 0;
L_007BA914:
    stk[--sp] = ebx;
    stk[--sp] = 83u;
    stk[--sp] = 83u;
    stk[--sp] = esi;
    stk[--sp] = 10u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    eax = edi;
    edi = stk[sp++];
    esi = stk[sp++];
    ebx = stk[sp++];
    return eax;
}
// 0x007BA970
std::uint32_t jetAttackMoveFollowWaypointMachineCtor(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    eax = stk[sp + 1];
    stk[--sp] = ebx;
    stk[--sp] = esi;
    stk[--sp] = edi;
    ebx = 0;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = eax;
    edi = ecx;
    { const std::uint32_t a[3] = {stk[sp + 0], stk[sp + 1], stk[sp + 2]}; eax = callExternal(env, 0x007AB710u, ecx, a, 3); }
    sp += 3;
    stk[--sp] = 60u;
    store32(asPtr(edi), 0, 12791264u);
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == ebx) goto L_007BA9D2;
    stk[--sp] = 102591889u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x38, ebx);
    store32(asPtr(esi), 0x20, 0);
    store32(asPtr(esi), 0x24, 0);
    store32(asPtr(esi), 0x28, 0);
    store32(asPtr(esi), 0x2c, 0);
    store32(asPtr(esi), 0x30, 0);
    store32(asPtr(esi), 0x34, 0);
    store32(asPtr(esi), 0, 12782544u);
    goto L_007BA9D4;
L_007BA9D2:
    esi = 0;
L_007BA9D4:
    stk[--sp] = ebx;
    stk[--sp] = 9999u;
    stk[--sp] = 104u;
    stk[--sp] = esi;
    stk[--sp] = 1002u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == ebx) goto L_007BAA0E;
    stk[--sp] = 1191984986u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0, 12757632u);
    goto L_007BAA10;
L_007BAA0E:
    esi = 0;
L_007BAA10:
    stk[--sp] = ebx;
    stk[--sp] = 86u;
    stk[--sp] = 9998u;
    stk[--sp] = esi;
    stk[--sp] = 104u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == ebx) goto L_007BAA4E;
    stk[--sp] = 1312888994u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0, 12774688u);
    store32(asPtr(esi), 0x20, 1u);
    goto L_007BAA50;
L_007BAA4E:
    esi = 0;
L_007BAA50:
    stk[--sp] = 12791212u;
    stk[--sp] = 9999u;
    stk[--sp] = 84u;
    stk[--sp] = esi;
    stk[--sp] = 86u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDC20u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 44u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == ebx) goto L_007BAA94;
    stk[--sp] = 1128509345u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x20, ebx);
    store32(asPtr(esi), 0, 12772200u);
    store32(asPtr(esi), 0x24, ebx);
    store32(asPtr(esi), 0x28, ebx);
    goto L_007BAA96;
L_007BAA94:
    esi = 0;
L_007BAA96:
    stk[--sp] = ebx;
    stk[--sp] = 83u;
    stk[--sp] = 83u;
    stk[--sp] = esi;
    stk[--sp] = 10u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    eax = edi;
    edi = stk[sp++];
    esi = stk[sp++];
    ebx = stk[sp++];
    return eax;
}
// 0x007BAB20
std::uint32_t jetAttackMoveToGoalMachineCtor(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    eax = stk[sp + 1];
    stk[--sp] = ebx;
    stk[--sp] = esi;
    stk[--sp] = edi;
    ebx = 0;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = 2559914944u;
    stk[--sp] = eax;
    edi = ecx;
    { const std::uint32_t a[4] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3]}; eax = callExternal(env, 0x005B6150u, ecx, a, 4); }
    sp += 4;
    stk[--sp] = 60u;
    store32(asPtr(edi), 0, 12791424u);
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == ebx) goto L_007BAB87;
    stk[--sp] = 102591889u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x38, ebx);
    store32(asPtr(esi), 0x20, 0);
    store32(asPtr(esi), 0x24, 0);
    store32(asPtr(esi), 0x28, 0);
    store32(asPtr(esi), 0x2c, 0);
    store32(asPtr(esi), 0x30, 0);
    store32(asPtr(esi), 0x34, 0);
    store32(asPtr(esi), 0, 12782544u);
    goto L_007BAB89;
L_007BAB87:
    esi = 0;
L_007BAB89:
    stk[--sp] = ebx;
    stk[--sp] = 9999u;
    stk[--sp] = 1u;
    stk[--sp] = esi;
    stk[--sp] = 1002u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == ebx) goto L_007BABC6;
    stk[--sp] = 531374569u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x20, ebx);
    store32(asPtr(esi), 0, 12770480u);
    goto L_007BABC8;
L_007BABC6:
    esi = 0;
L_007BABC8:
    stk[--sp] = 12791388u;
    stk[--sp] = 9999u;
    stk[--sp] = 9998u;
    stk[--sp] = esi;
    stk[--sp] = 1u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 44u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == ebx) goto L_007BAC0F;
    stk[--sp] = 1128509345u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x20, ebx);
    store32(asPtr(esi), 0, 12772200u);
    store32(asPtr(esi), 0x24, ebx);
    store32(asPtr(esi), 0x28, ebx);
    goto L_007BAC11;
L_007BAC0F:
    esi = 0;
L_007BAC11:
    stk[--sp] = ebx;
    stk[--sp] = 1u;
    stk[--sp] = 1u;
    stk[--sp] = esi;
    stk[--sp] = 10u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    eax = edi;
    edi = stk[sp++];
    esi = stk[sp++];
    ebx = stk[sp++];
    return eax;
}
// 0x007BB2B0
std::uint32_t jetMoveToGoalMachineCtor(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    eax = stk[sp + 1];
    stk[--sp] = esi;
    stk[--sp] = edi;
    stk[--sp] = 0u;
    stk[--sp] = 1u;
    stk[--sp] = 1586248360u;
    stk[--sp] = eax;
    esi = ecx;
    { const std::uint32_t a[4] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3]}; eax = callExternal(env, 0x005B6150u, ecx, a, 4); }
    sp += 4;
    stk[--sp] = 60u;
    store32(asPtr(esi), 0, 12792328u);
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == 0) goto L_007BB31A;
    stk[--sp] = 102591889u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x38, 0u);
    store32(asPtr(edi), 0x20, 0);
    store32(asPtr(edi), 0x24, 0);
    store32(asPtr(edi), 0x28, 0);
    store32(asPtr(edi), 0x2c, 0);
    store32(asPtr(edi), 0x30, 0);
    store32(asPtr(edi), 0x34, 0);
    store32(asPtr(edi), 0, 12782544u);
    goto L_007BB31C;
L_007BB31A:
    edi = 0;
L_007BB31C:
    stk[--sp] = 0u;
    stk[--sp] = 9999u;
    stk[--sp] = 104u;
    stk[--sp] = edi;
    stk[--sp] = 1002u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == 0) goto L_007BB357;
    stk[--sp] = 1191984986u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0, 12757632u);
    goto L_007BB359;
L_007BB357:
    edi = 0;
L_007BB359:
    stk[--sp] = 0u;
    stk[--sp] = 83u;
    stk[--sp] = 9998u;
    stk[--sp] = edi;
    stk[--sp] = 104u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == 0) goto L_007BB391;
    stk[--sp] = 481294402u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0, 12761272u);
    goto L_007BB393;
L_007BB391:
    edi = 0;
L_007BB393:
    stk[--sp] = 0u;
    stk[--sp] = 9999u;
    stk[--sp] = 85u;
    stk[--sp] = edi;
    stk[--sp] = 83u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == 0) goto L_007BB3CB;
    stk[--sp] = 2612172156u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0, 12758032u);
    goto L_007BB3CD;
L_007BB3CB:
    edi = 0;
L_007BB3CD:
    stk[--sp] = 0u;
    stk[--sp] = 9999u;
    stk[--sp] = 86u;
    stk[--sp] = edi;
    stk[--sp] = 85u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == 0) goto L_007BB40C;
    stk[--sp] = 1312888994u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0, 12774688u);
    store32(asPtr(edi), 0x20, 1u);
    goto L_007BB40E;
L_007BB40C:
    edi = 0;
L_007BB40E:
    stk[--sp] = 12792292u;
    stk[--sp] = 9999u;
    stk[--sp] = 9998u;
    stk[--sp] = edi;
    stk[--sp] = 86u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    edi = stk[sp++];
    eax = esi;
    esi = stk[sp++];
    return eax;
}
// 0x007BB500
std::uint32_t moveToRallyPointMachineCtor(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    eax = stk[sp + 1];
    stk[--sp] = esi;
    stk[--sp] = edi;
    stk[--sp] = 0u;
    stk[--sp] = 1u;
    stk[--sp] = 2604700353u;
    stk[--sp] = eax;
    edi = ecx;
    { const std::uint32_t a[4] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3]}; eax = callExternal(env, 0x005B6150u, ecx, a, 4); }
    sp += 4;
    stk[--sp] = 60u;
    store32(asPtr(edi), 0, 12792432u);
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == 0) goto L_007BB56A;
    stk[--sp] = 102591889u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x38, 0u);
    store32(asPtr(esi), 0x20, 0);
    store32(asPtr(esi), 0x24, 0);
    store32(asPtr(esi), 0x28, 0);
    store32(asPtr(esi), 0x2c, 0);
    store32(asPtr(esi), 0x30, 0);
    store32(asPtr(esi), 0x34, 0);
    store32(asPtr(esi), 0, 12782544u);
    goto L_007BB56C;
L_007BB56A:
    esi = 0;
L_007BB56C:
    stk[--sp] = 0u;
    stk[--sp] = 9999u;
    stk[--sp] = 1u;
    stk[--sp] = esi;
    stk[--sp] = 1002u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 36u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == 0) goto L_007BB5AE;
    stk[--sp] = 1678529186u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x20, 0u);
    store32(asPtr(esi), 0, 12775232u);
    goto L_007BB5B0;
L_007BB5AE:
    esi = 0;
L_007BB5B0:
    stk[--sp] = 0u;
    stk[--sp] = 9999u;
    stk[--sp] = 9998u;
    stk[--sp] = esi;
    stk[--sp] = 1u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    eax = edi;
    edi = stk[sp++];
    esi = stk[sp++];
    return eax;
}
// 0x007BBD20
std::uint32_t attackMoveFollowPathMachineCtor(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    eax = stk[sp + 1];
    stk[--sp] = esi;
    stk[--sp] = edi;
    stk[--sp] = eax;
    edi = ecx;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x007AB5B0u, ecx, a, 1); }
    sp += 1;
    stk[--sp] = 36u;
    store32(asPtr(edi), 0, 12792784u);
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == 0) goto L_007BBD60;
    stk[--sp] = 1312888994u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0, 12774688u);
    store32(asPtr(esi), 0x20, 1u);
    goto L_007BBD62;
L_007BBD60:
    esi = 0;
L_007BBD62:
    stk[--sp] = 12792736u;
    stk[--sp] = 9999u;
    stk[--sp] = 84u;
    stk[--sp] = esi;
    stk[--sp] = 86u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDC20u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 44u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == 0) goto L_007BBDB2;
    stk[--sp] = 1128509345u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x20, 0u);
    store32(asPtr(esi), 0, 12772200u);
    store32(asPtr(esi), 0x24, 0u);
    store32(asPtr(esi), 0x28, 0u);
    goto L_007BBDB4;
L_007BBDB2:
    esi = 0;
L_007BBDB4:
    stk[--sp] = 0u;
    stk[--sp] = 83u;
    stk[--sp] = 83u;
    stk[--sp] = esi;
    stk[--sp] = 10u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    eax = edi;
    edi = stk[sp++];
    esi = stk[sp++];
    return eax;
}
// 0x007BBE40
std::uint32_t attackMoveFollowWaypointMachineCtor(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    eax = stk[sp + 1];
    stk[--sp] = esi;
    stk[--sp] = edi;
    stk[--sp] = 0u;
    stk[--sp] = 0u;
    stk[--sp] = eax;
    edi = ecx;
    { const std::uint32_t a[3] = {stk[sp + 0], stk[sp + 1], stk[sp + 2]}; eax = callExternal(env, 0x007AB710u, ecx, a, 3); }
    sp += 3;
    stk[--sp] = 36u;
    store32(asPtr(edi), 0, 12792952u);
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == 0) goto L_007BBE84;
    stk[--sp] = 1312888994u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0, 12774688u);
    store32(asPtr(esi), 0x20, 1u);
    goto L_007BBE86;
L_007BBE84:
    esi = 0;
L_007BBE86:
    stk[--sp] = 12792904u;
    stk[--sp] = 9999u;
    stk[--sp] = 84u;
    stk[--sp] = esi;
    stk[--sp] = 86u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDC20u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 44u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == 0) goto L_007BBED6;
    stk[--sp] = 1128509345u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x20, 0u);
    store32(asPtr(esi), 0, 12772200u);
    store32(asPtr(esi), 0x24, 0u);
    store32(asPtr(esi), 0x28, 0u);
    goto L_007BBED8;
L_007BBED6:
    esi = 0;
L_007BBED8:
    stk[--sp] = 0u;
    stk[--sp] = 83u;
    stk[--sp] = 83u;
    stk[--sp] = esi;
    stk[--sp] = 10u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    eax = edi;
    edi = stk[sp++];
    esi = stk[sp++];
    return eax;
}
// 0x007BBF30
std::uint32_t attackMoveToGoalMachineCtor(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    eax = stk[sp + 1];
    stk[--sp] = ebx;
    stk[--sp] = esi;
    stk[--sp] = edi;
    ebx = 0;
    stk[--sp] = ebx;
    stk[--sp] = ebx;
    stk[--sp] = 2991543619u;
    stk[--sp] = eax;
    edi = ecx;
    { const std::uint32_t a[4] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3]}; eax = callExternal(env, 0x005B6150u, ecx, a, 4); }
    sp += 4;
    stk[--sp] = 36u;
    store32(asPtr(edi), 0, 12793112u);
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == ebx) goto L_007BBF76;
    stk[--sp] = 1678529186u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x20, ebx);
    store32(asPtr(esi), 0, 12775232u);
    goto L_007BBF78;
L_007BBF76:
    esi = 0;
L_007BBF78:
    stk[--sp] = 12793076u;
    stk[--sp] = 9999u;
    stk[--sp] = 9998u;
    stk[--sp] = esi;
    stk[--sp] = 1u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 44u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    esi = eax;
    sp += 1;
    if (esi == ebx) goto L_007BBFBF;
    stk[--sp] = 1128509345u;
    stk[--sp] = edi;
    ecx = esi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(esi), 0x20, ebx);
    store32(asPtr(esi), 0, 12772200u);
    store32(asPtr(esi), 0x24, ebx);
    store32(asPtr(esi), 0x28, ebx);
    goto L_007BBFC1;
L_007BBFBF:
    esi = 0;
L_007BBFC1:
    stk[--sp] = ebx;
    stk[--sp] = 1u;
    stk[--sp] = 1u;
    stk[--sp] = esi;
    stk[--sp] = 10u;
    ecx = edi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    eax = edi;
    edi = stk[sp++];
    esi = stk[sp++];
    ebx = stk[sp++];
    return eax;
}
// 0x007BC100
std::uint32_t combinedUnitMachineCtor(std::uint8_t* self, std::uint32_t eaxIn, const std::uint32_t* stack, int argc, AIState7BEnv& env)
{
    std::uint32_t eax = eaxIn, ecx = addressOf(self), edx = 0, ebx = 0, esi = 0, edi = 0;
    std::uint32_t stk[64] = {};
    int sp = 32;
    for (int i = 0; i < argc && i < 8; ++i) { stk[33 + i] = stack[i]; }
    (void)eax; (void)ecx; (void)edx; (void)ebx; (void)esi; (void)edi; (void)stk; (void)sp; (void)env;
    eax = stk[sp + 1];
    stk[--sp] = esi;
    stk[--sp] = edi;
    stk[--sp] = 0u;
    stk[--sp] = 1u;
    stk[--sp] = 3917146359u;
    stk[--sp] = eax;
    esi = ecx;
    { const std::uint32_t a[4] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3]}; eax = callExternal(env, 0x005B6150u, ecx, a, 4); }
    sp += 4;
    stk[--sp] = 36u;
    store32(asPtr(esi), 0, 12793216u);
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == 0) goto L_007BC149;
    stk[--sp] = 1678529186u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0x20, 0u);
    store32(asPtr(edi), 0, 12783688u);
    goto L_007BC14B;
L_007BC149:
    edi = 0;
L_007BC14B:
    stk[--sp] = 0u;
    stk[--sp] = 9999u;
    stk[--sp] = 0u;
    stk[--sp] = edi;
    stk[--sp] = 1u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    stk[--sp] = 32u;
    { const std::uint32_t a[1] = {stk[sp + 0]}; eax = callExternal(env, 0x00416820u, ecx, a, 1); }
    edi = eax;
    sp += 1;
    if (edi == 0) goto L_007BC183;
    stk[--sp] = 284512755u;
    stk[--sp] = esi;
    ecx = edi;
    { const std::uint32_t a[2] = {stk[sp + 0], stk[sp + 1]}; eax = callExternal(env, 0x005A9F60u, ecx, a, 2); }
    sp += 2;
    store32(asPtr(edi), 0, 12757928u);
    goto L_007BC185;
L_007BC183:
    edi = 0;
L_007BC185:
    stk[--sp] = 0u;
    stk[--sp] = 9999u;
    stk[--sp] = 9999u;
    stk[--sp] = edi;
    stk[--sp] = 0u;
    ecx = esi;
    { const std::uint32_t a[5] = {stk[sp + 0], stk[sp + 1], stk[sp + 2], stk[sp + 3], stk[sp + 4]}; eax = callExternal(env, 0x005BDB90u, ecx, a, 5); }
    sp += 5;
    edi = stk[sp++];
    eax = esi;
    esi = stk[sp++];
    return eax;
}
// 0x007BA230
std::uint32_t unitAIDisguiseAggressiveSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto created = call1(env, 0x00416820u, addressOf(self), 128);
    if (created == 0) {
        return 0;
    }
    const auto unit = load32(asPtr(load32(self, 0x18)), 0x24);
    return disguiseAggressiveMachineCtor(asPtr(created), 0, &unit, 1, env);
}

// 0x007BA3E0
std::uint32_t unitAIHeliEvacuateSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto created = call1(env, 0x00416820u, addressOf(self), 128);
    if (created == 0) {
        return 0;
    }
    const auto unit = load32(asPtr(load32(self, 0x18)), 0x24);
    return heliEvacuateMachineCtor(asPtr(created), 0, &unit, 1, env);
}

// 0x007BA940
std::uint32_t jetAIAttackMoveFollowPathSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto created = call1(env, 0x00416820u, addressOf(self), 132);
    if (created == 0) {
        return 0;
    }
    const auto unit = load32(asPtr(load32(self, 0x18)), 0x24);
    return jetAttackMoveFollowPathMachineCtor(asPtr(created), 0, &unit, 1, env);
}

// 0x007BAAF0
std::uint32_t jetAIAttackMoveFollowWaypointSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto created = call1(env, 0x00416820u, addressOf(self), 152);
    if (created == 0) {
        return 0;
    }
    const auto unit = load32(asPtr(load32(self, 0x18)), 0x24);
    return jetAttackMoveFollowWaypointMachineCtor(asPtr(created), 0, &unit, 1, env);
}

// 0x007BAC70
std::uint32_t jetAIAttackMoveToGoalSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto created = call1(env, 0x00416820u, addressOf(self), 128);
    if (created == 0) {
        return 0;
    }
    const auto unit = load32(asPtr(load32(self, 0x18)), 0x24);
    return jetAttackMoveToGoalMachineCtor(asPtr(created), 0, &unit, 1, env);
}

// 0x007BB470
std::uint32_t jetAIMoveToGoalSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto created = call1(env, 0x00416820u, addressOf(self), 128);
    if (created == 0) {
        return 0;
    }
    const auto unit = load32(asPtr(load32(self, 0x18)), 0x24);
    return jetMoveToGoalMachineCtor(asPtr(created), 0, &unit, 1, env);
}

// 0x007BB610
std::uint32_t unitAIMoveToRallyPointSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto created = call1(env, 0x00416820u, addressOf(self), 128);
    if (created == 0) {
        return 0;
    }
    const auto unit = load32(asPtr(load32(self, 0x18)), 0x24);
    return moveToRallyPointMachineCtor(asPtr(created), 0, &unit, 1, env);
}

// 0x007BBE10
std::uint32_t unitAIAttackMoveFollowPathSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto created = call1(env, 0x00416820u, addressOf(self), 132);
    if (created == 0) {
        return 0;
    }
    const auto unit = load32(asPtr(load32(self, 0x18)), 0x24);
    return attackMoveFollowPathMachineCtor(asPtr(created), 0, &unit, 1, env);
}

// 0x007BBF00
std::uint32_t unitAIAttackMoveFollowWaypointSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto created = call1(env, 0x00416820u, addressOf(self), 152);
    if (created == 0) {
        return 0;
    }
    const auto unit = load32(asPtr(load32(self, 0x18)), 0x24);
    return attackMoveFollowWaypointMachineCtor(asPtr(created), 0, &unit, 1, env);
}

// 0x007BC020
std::uint32_t unitAIAttackMoveToGoalSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto created = call1(env, 0x00416820u, addressOf(self), 128);
    if (created == 0) {
        return 0;
    }
    const auto unit = load32(asPtr(load32(self, 0x18)), 0x24);
    return attackMoveToGoalMachineCtor(asPtr(created), 0, &unit, 1, env);
}

// 0x007BC1C0
std::uint32_t unitAICombinedUnitSlot19(std::uint8_t* self, std::uint32_t, const std::uint32_t*, int, AIState7BEnv& env)
{
    const auto created = call1(env, 0x00416820u, addressOf(self), 128);
    if (created == 0) {
        return 0;
    }
    const auto unit = load32(asPtr(load32(self, 0x18)), 0x24);
    return combinedUnitMachineCtor(asPtr(created), 0, &unit, 1, env);
}

