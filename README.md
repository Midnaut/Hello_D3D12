# Hello_D3D12

This is inteded as a learning project for getting my head around the dx12 samples, namely the [D3D12HelloWorld](https://github.com/microsoft/DirectX-Graphics-Samples/tree/master/Samples/Desktop/D3D12HelloWorld) project.

I found myself extremely unhappy with the level of abstraction, so spent extra time going over the helper classes, and de-abstracting them all. This allowed for much greater understanding while consumed in tandem with [3D Game engine progamming's DirectX12 Articles](https://www.3dgep.com/learning-directx-12-1/), [Raw DirectX12 by Alain Galvan](https://alain.xyz/blog/raw-directx12) and the [Game Engine Series DirectX 12 Renderer playlist](https://www.youtube.com/watch?v=mrxTQAtNFuc&list=PLU2nPsAdxKWQw1qBS9YdFi9hUMazppjV7&ab_channel=GameEngineSeries).


As you will be able to see, I have deviated from them somewhat by aslo taking some direction from each to my liking. For this i eschewed the suggested window implementations and used my own windowing/app framework that uses SDL2 [lifted from my 2D engine](https://github.com/Midnaut/Acid-2D). Also instead of seperate projects, I opted to attempt to combine all of the examples that were relevant, namely Hello Triangle/Texture/Bundles/Constant Buffers and Frame Buffering. Hello Window was skipped.

Notes:
- This is purely an academic exercise and as such no effort has gone into a release build setup. everything has been set up and assumed to be using [SDL2 x64 Debug](https://github.com/Midnaut/Prebuillt-x64-Debug-SDL2)

- This in no way should be used as a starting point for any work. If you want to build something clean, from scratch, with much better architecture I suggest the [3D Game engine progamming's DirectX12 Articles](https://www.3dgep.com/learning-directx-12-1/)


Special thanks: My DX12 sherpa - [Eynx](https://github.com/Eynx)
