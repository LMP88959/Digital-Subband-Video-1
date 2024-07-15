const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});

    // Create the executable
    const bin = b.addExecutable(.{
        .name = "dsv1",
        .target = target,
        .optimize = .ReleaseFast,
    });

    // Add C source files
    bin.addCSourceFiles(.{
        .files = &.{
            "bmc.c",
            "bs.c",
            "dsv.c",
            "dsv_decoder.c",
            "dsv_encoder.c",
            "dsv_main.c",
            "frame.c",
            "hme.c",
            "hzcc.c",
            "sbt.c",
            "util.c",
        },
        .flags = &.{
            "-std=c99",
            "-O3",
        },
    });

    // Link libc
    bin.linkLibC();
    b.installArtifact(bin);
}
