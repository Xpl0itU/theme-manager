FROM ghcr.io/wiiu-env/devkitppc:20230621

COPY --from=wiiuenv/libmocha:20230621 /artifacts $DEVKITPRO

WORKDIR /project
