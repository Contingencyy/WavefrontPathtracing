#ifndef GROUP_THREADS_X
#define GROUP_THREADS_X 16
#endif

#ifndef GROUP_THREADS_Y
#define GROUP_THREADS_Y 16
#endif

[numthreads(GROUP_THREADS_X, GROUP_THREADS_Y, 1)]
void main(uint3 dispatch_id : SV_DispatchThreadID)
{
    uint2 pixel_pos = dispatch_id.xy;
    
    RWTexture2D<float4> render_target = ResourceDescriptorHeap[NonUniformResourceIndex(0)];
    render_target[pixel_pos] = float4(1.0f, 0.0f, 1.0f, 1.0f);
}
