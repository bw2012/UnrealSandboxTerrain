# UE4/UE5 Procedural smooth voxel terrain plugin 

Provide high terrain detalization with high rendering performance

![Unreal Engine 4 voxel terrian](http://media.indiedb.com/images/games/1/51/50197/ezgif.com-video-to-gif_2.gif)

# Compatibility

Tested with Unreal Engine 5.4, 5.3, 5.2, 5.1, 4.27 on Ubuntu Linux 22.04/24.02 and Windows 10

> [!WARNING]
Unreal Engine 5.5 is not officially supported yet due to multithreading performance problem with UE 5.5 and Win 10. Work in progress

> [!WARNING]  
> Blueprint is not supported. C++ projects only. Please contact with author if you need to integration with your BP project

# Key features
* Runtime terrain modification
* Procedural landscape/caves generation
* Level of details
* Up to 65535 terrain material (dirt, grass, sand, clay etc)
* Network multiplayer support

# Dependencies

Required [UnrealSandoxData](https://github.com/bw2012/UnrealSandboxData) plugin

# Example
Example UE4 project (discontinued) - https://github.com/bw2012/UE4VoxelTerrain

Example UE5 project (with grass and trees) - https://github.com/bw2012/UE5VoxelTerrainDemo

Example UE5 project (minimal working example)- https://github.com/bw2012/UE5VoxelTerrainTemplate

# Credits

Partially based on Transvoxel™ Algorithm by Eric Lengyel 

http://transvoxel.org/ 

https://github.com/EricLengyel/Transvoxel

> Lengyel, Eric. “Voxel-Based Terrain for Real-Time Virtual Simulations”. PhD diss., University of California at Davis, 2010.

# License
MIT license

