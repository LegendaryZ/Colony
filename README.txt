Colony is a highly parallel optimized crowd simulation technique. Tens of thousands 
of units are simulated using a novel ray-casting technique. This is achieved by 
utilizing TBB to distribute our work across multiple threads and multiple frames 
and utilizing SIMD to ensure further instruction-level parallelism.


== Build instructions ==

Open Colony/Colony_{2008, 2010}.sln


== System Requirements ==

Hardware:
 - CPU: Dual core or better (Intel® Core™ i5 or better suggested)
 - GPU: DX10 capable graphics card
 - OS: Microsoft Windows Vista* or Microsoft Windows 7*
 - Memory: 2 GB of RAM or better
Software:
 - DirectX SDK (June 2010 release or later)
 - Microsoft Visual Studio 2008* w/SP1 or Visual Studio 2010*
