# Digital-Subband-Video-1
------

DSV1 is a simple video codec using wavelets and block-based motion compensation.  
It is comparable to MPEG-1/MPEG-2 in terms of efficiency and quality.

## DSV Features (refer to PDF in repo for more detail)

- compression using multiresolution subband analysis instead of DCT  
   - also known as a wavelet transform  
- half-pixel motion compensation  
- 4:1:1, 4:2:0, 4:2:2 and 4:4:4 chroma subsampling formats  
- adaptive quantization  
- intra and inter frames with variable length closed GOP  
   - no bidirectional prediction (also known as B-frames). Only forward prediction with previous picture as reference  
- only form of entropy coding is interleaved exponential-golomb coding for simplicity  
- wider range of compression than MPEG-1/2, support for lower bitrates

--- for more detailed information please refer to the informal specification document (DSV1_spec.pdf) in the repository.

## Encoder Features (specific to this implementation of the DSV spec)

- single pass average bitrate (ABR) or constant rate factor (CRF) rate control  
- simple Human Visual System (HVS) based intra block mode determination  
- simple scene change detection using change in average luma  
- hierarchical motion estimation  
- stability tracking to provide better adaptive quantization  
- written to be compatible with C89  

--- for more detailed information please refer to the encoder information document (DSV1_encoder.pdf) in the repository.

## Limitations

- no built-in interlacing support  
- only 8 bits of depth per component supported  
- frame sizes must be divisible by two  

This code follows my self-imposed restrictions:

1. Everything must be done in software, no explicit usage of hardware acceleration.
2. No floating point types or literals, everything must be integer only.
3. No 3rd party libraries, only C standard library and OS libraries for window, input, etc.
4. No languages used besides C.
5. No compiler specific features and no SIMD.
6. Single threaded.

## Compiling

All you need is a C compiler.

In the root directory of the project (with all the .h and .c files):
```bash
cc -O3 -o dsv1 *.c
```


## Running Encoder

Sample output:
```
Envel Graphics DSV v1.0 compliant codec by EMMIR 2023-2024
usage: ./dsv1 e [options]
sample usage: ./dsv1 -inp_video.yuv -out_compressed.dsv -w352 -h288 -fps_num24 -fps_den1 -qp85 -gop15
------------------------------------------------------------
        -qp : quality percent. 85 = default
              [min = 0, max = 100]
        -w : width of input video. 352 = default
              [min = 16, max = 16777216]
        -h : height of input video. 288 = default
              [min = 16, max = 16777216]
        -gop : Group Of Pictures length. 0 = intra frames only, 12 = default
              [min = 0, max = 2147483647]
        -fmt : chroma subsampling format of input video. 0 = 4:4:4, 1 = 4:2:2, 2 = 4:2:0, 3 = 4:1:1, 2 = default
              [min = 0, max = 3]
        -nfr : number of frames to compress. -1 means as many as possible. -1 = default
              [min = -1, max = 2147483647]
        -sfr : frame number to start compressing at. 0 = default
              [min = 0, max = 2147483647]
        -fps_num : fps numerator of input video. 30 = default
              [min = 1, max = 16777216]
        -fps_den : fps denominator of input video. 1 = default
              [min = 1, max = 16777216]
        -aspect_num : aspect ratio numerator of input video. 1 = default
              [min = 1, max = 16777216]
        -aspect_den : aspect ratio denominator of input video. 1 = default
              [min = 1, max = 16777216]
        -ipct : percentage threshold of intra blocks in an inter frame after which it is simply made into an intra frame. 50 = default
              [min = 0, max = 100]
        -pyrlevels : number of pyramid levels to use in hierarchical motion estimation. 0 means auto-determine. 0 = default
              [min = 0, max = 5]
        -rc_mode : rate control mode. 0 = single pass average bitrate (ABR), 1 = constant rate factor (CRF). 0 = default
              [min = 0, max = 1]
        -rc_hmnudge : nudge the rate control loop a bit harder in high motion scenes. 1 = default
              [min = 0, max = 1]
        -kbps : ONLY FOR ABR RATE CONTORL: bitrate in kilobits per second. 0 = auto-estimate needed bitrate for desired qp. 0 = default
              [min = 0, max = 2147483647]
        -maxqstep : max quality step for ABR, absolute quant amount. 10 = default (equivalent to 0.5%)
              [min = 1, max = 2047]
        -minqp : minimum quality percent. 1 = default
              [min = 0, max = 100]
        -maxqp : maximum quality percent. 95 = default
              [min = 0, max = 100]
        -minqp_I : minimum quality percent for intra frames. 5 = default
              [min = 0, max = 100]
        -stabref : period (in # of frames) to refresh the stability block tracking. 0 = auto-determine. 0 = default
              [min = 0, max = 2147483647]
        -scd : do scene change detection. 1 = default
              [min = 0, max = 1]
        -scdelta : scene change average luma delta threshold. Units are 8-bit luma.4 = default
              [min = 0, max = 256]
        -inp_ : REQUIRED! input file
        -out_ : REQUIRED! output file
        -y : do not prompt for confirmation when potentially overwriting an existing file
        -l<n> : set logging level to n (0 = none, 1 = error, 2 = warning, 3 = info, 4 = debug/all)
        -v : set verbose
```

## Running Decoder

Sample output:
```
Envel Graphics DSV v1.0 compliant codec by EMMIR 2023-2024
usage: ./dsv1 d [options]
sample usage: ./dsv1 -inp_video.dsv -out_decompressed.yuv -out420p
------------------------------------------------------------
        -out420p : convert video to 4:2:0 chroma subsampling before saving output. 0 = default
              [min = 0, max = 1]
        -drawinfo : draw debugging information on the decoded frames (bit OR together to get multiple at the same time):
                1 = draw stability info
                2 = draw motion vectors
                4 = draw intra subblocks. 0 = default
              [min = 0, max = 7]
        -inp_ : REQUIRED! input file
        -out_ : REQUIRED! output file
        -y : do not prompt for confirmation when potentially overwriting an existing file
        -l<n> : set logging level to n (0 = none, 1 = error, 2 = warning, 3 = info, 4 = debug/all)
        -v : set verbose
```

------
NOTE: -inp_ and -out_ must be specified. Only .yuv files (one file containing all the frames) are supported as inputs to the encoder.

All arguments have no space between the -identifier and value, e.g -identifiervalue

------

## Notes

This codec is by no means fully optimized, so there is a lot of room for performance gains. It performs quite well for what it is though.

## License

The accompanying software was designed and written by EMMIR, 2023-2024 of Envel Graphics.
No responsibility is assumed by the author.

Feel free to use the code in any way you would like, however, if you release anything with it, a comment in your code/README document saying where you got this code would be a nice gesture but it is not mandatory.
The software is provided "as is", without warranty of any kind, express or implied, including but not limited to the warranties of merchantability, fitness for a particular purpose and noninfringement. In no event shall the authors or copyright holders be liable for any claim, damages or other liability, whether in an action of contract, tort or otherwise, arising from, out of or in connection with the software or the use or other dealings in the software.

------
If you have any questions feel free to leave a comment on YouTube OR
join the King's Crook Discord server :)

YouTube: https://www.youtube.com/@EMMIR_KC/videos

Discord: https://discord.gg/hdYctSmyQJ

itch.io: https://kingscrook.itch.io/kings-crook
