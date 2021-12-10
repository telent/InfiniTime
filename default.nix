with import <nixpkgs> { };# crossSystem = "arm-none-unknown-eabi"; } ;


let nrf5Sdk = pkgs.fetchzip {
      name = "nRF5_SDK_15.3.0";
      url = "https://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v15.x.x/nRF5_SDK_15.3.0_59ac345.zip";
      sha256 = "0kwgafa51idn0cavh78zgakb02xy49vzag7firv9rgqmk1pa3yd5";

    };
    pkgsArm =  import <nixpkgs> { crossSystem = { system = "arm-none-eabi"; } ; };
    python = pkgs.python38.withPackages(ps: with ps; [ cbor intelhex click cryptography setuptools ]);

    lua = pkgsArm.stdenv.mkDerivation {
      pname = "lua";
      version = "5.3";
      configurePhase = ''
        sed -i src/luaconf.h \
          -e '/define LUA_32BITS/c\#define LUA_32BITS' \
          -e '/define LUA_COMPAT_UNPACK/c\#undef LUA_COMPAT_UNPACK' \
          -e '/define LUA_COMPAT_LOADERS/c\#undef LUA_COMPAT_LOADERS' \
          -e '/define LUA_COMPAT_LOG10/c\#undef LUA_COMPAT_LOG10' \
          -e '/define LUA_COMPAT_LOADSTRING/c\#undef LUA_COMPAT_LOADSTRING' \
          -e '/define LUA_COMPAT_MODULE/c\#undef LUA_COMPAT_MODULE' \
          -e '/define LUA_COMPAT_MAXN/c\#undef LUA_COMPAT_MAXN'
      '';

      buildPhase =
        let flags="-Os -mthumb -mabi=aapcs -ffunction-sections -fdata-sections -fno-strict-aliasing -fno-builtin --short-enums -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16";
        in ''
          make -C src liblua.a CC=$CC CFLAGS=${builtins.toJSON flags} AR="$AR rcu" RANLIB="$RANLIB"
        '';
      installPhase = ''
        mkdir -p $out/lib $out/include
        cp src/liblua.a $out/lib
        cp src/{lua,luaconf,lualib,lauxlib}.h $out/include
      '';

      src = builtins.fetchTarball {
        url = "https://www.lua.org/ftp/lua-5.3.6.tar.gz";
        name = "lua";
        sha256 = "1dp74vnqznvdniqp0is1l55v96kx5yshkgcmzcwcfqzadkzjs0ds";
      };
    };
    infinitime = stdenv.mkDerivation {
      src = if lib.inNixShell
            then null
            else builtins.path {
              name = "infinitime";
              path = ./.;
              filter = path: type:
                let relpath = lib.removePrefix (builtins.toString ./.) path;
                in !(pkgs.lib.hasPrefix "/build/" relpath);
            };
      pname = "infinitime";
      version = "1.7.1-plus";
      cmakeFlags = [
        "-DARM_NONE_EABI_TOOLCHAIN_PATH=${gcc-arm-embedded}"
        "-DNRF5_SDK_PATH=${nrf5Sdk}"
        "-DLUA_PATH=${lua}"
        "-DCMAKE_BUILD_TYPE=Debug"
        "-DUSE_GDB_CLIENT=1"
        "-DGDB_CLIENT_TARGET_REMOTE=/dev/ttyACM0"
#        "-DCMAKE_VERBOSE_MAKEFILE=1"
      ];
      makeFlags = [
#        "pinetime-app"
        "pinetime-recovery"
      ];
      installPhase = ''
    mkdir -p $out/lib
    cp src/*.{bin,hex,map,out} $out/lib
  '';
      shellHook = ''
    cleanish() { git  clean -f ; git clean -fdX ; }
  '';

      nativeBuildInputs =  [ gcc-arm-embedded python pkgs.cmake ];
      buildInputs = [
        nrf5Sdk
        #    lua
      ];
    };
in infinitime
