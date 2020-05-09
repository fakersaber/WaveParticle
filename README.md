# WaveParticle

**Mobile效果**：

![image](assets/water.gif)



**#TODO**

- [ ] FrustumCull
  - **UpdateBounds** Or **CalculateBounds** override
- [ ] OcclusionCull 
  - Procudual Mesh ?   X  
  - Bound Box ? 
- [ ] Lod
  - Static Mesh Setting
- Tip1：边缘情况使用材质参数计算，不创建额外资源，保证波形连续。
- Tip2：使用CS同时对Resources操作，使用interlock可以跨group（GL需要拓展）。





