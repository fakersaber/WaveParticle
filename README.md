WaveParticle

**Mobile效果**：

![Ocean](assets/Ocean.gif)

**#TODO**

- [x] Quad Tree FrustumCull
- [x] OcclusionCull ，Use Soft Occlusion
- [x] Lod，边缘离线生成Static Mesh Setting
- [x] Foam，Jocbi
- [x] 支持Transform的更新（现在未跟随ParticleMa）
- [ ] IOS Crash
- Tip1：边缘情况使用材质参数计算，不创建额外资源，保证波形连续。
- Tip2：使用CS同时对Resources操作，使用interlock可以跨group（GL需要拓展）。
- Tip3：开启Dynamic Instance后，每个Pass会多出一张巨大的GPUScene



**#Annotate**

原Paper在粒子边界处波形函数会发生突变：
$$f(x) = x - \beta \ \mathrm{sin}x $$，替换为$$f(x) = \mathrm{sin}x$$

