PhysicsFS with MPQ file support.

A built-in MPQ v1 implementation is provided based on StormLib
by Ladislav Zezula.

https://github.com/ladislav-zezula/StormLib

The changes are mainly in physfs_archiver_mpq.c
Only read only support is implemented.

To use an external (full featured) StormLib, run like this:

cmake CMakeLists.txt -DPHYSFS_USE_EXTERNAL_STORMLIB:BOOL=TRUE

On Windows, to use an external StormLib.dll (ANSI) instead
of the built-in implementation, run like this:

cmake CMakeLists.txt -PHYSFS_USE_EXTERNAL_STORMDLL:BOOL=TRUE

If StormLib.dll isn't found, the built-in implementation is used.
