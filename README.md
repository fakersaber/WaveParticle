# WaveParticle

**Mobile效果**：

![water](assets/water.gif)

![Water_2](assets/Water_2.gif)



**#TODO**

- [x] FrustumCull
  - **UpdateBounds** Or **CalcBounds** override
- [x] OcclusionCull 
  - Use Soft Occlusion
- [x] Lod
  - Static Mesh Setting
- [x] Foam
  - Jocbi
- Tip1：边缘情况使用材质参数计算，不创建额外资源，保证波形连续。
- Tip2：使用CS同时对Resources操作，使用interlock可以跨group（GL需要拓展）。
- Tip3：开启Dynamic Instance后，每个Pass会多出一张巨大的GPUScene





