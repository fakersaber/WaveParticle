# WaveParticle

- [ ] FrustumCull
  - **UpdateBounds** Or **CalculateBounds** override
- [ ] OcclusionCull 
  - Procudual Mesh ?   X  
  - Bound Box ? 
- [ ] Lod
  - Static Mesh Setting
- Tip1：使用一张向量场Texture叠加波，仅仅改变UV，不使用多Mesh与多Texture（包括Mipmap）
- Tip2：边缘情况使用材质参数计算，不创建额外资源。
- Tip3：使用cs同时对Resources操作，使用interlock可以跨group



GPU版本步骤：

- 





