#include <algorithm>
#include <cfloat>
#include <vector>

#include "caffe/layers/kl_loss_layer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {

template <typename Dtype>
void KLLossLayer<Dtype>::Forward_gpu(
    const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
  const Dtype* P_data = bottom[0]->gpu_data();
    const Dtype* Q_data = bottom[1]->gpu_data();
    Dtype* temp = temp_.mutable_gpu_data();
    //P/Q
    caffe_gpu_div(bottom[0]->count(), P_data, Q_data, temp);
        //compute gradients w.r.t Q
        caffe_gpu_axpby(bottom[0]->count(), Dtype(-1), temp_.gpu_data(), Dtype(0), bottom[1]->mutable_gpu_diff());
    //log(P/Q)
    caffe_gpu_log(bottom[0]->count(), temp_.gpu_data(), temp);
        //compute gradients w.r.t P
        caffe_gpu_add_scalar(bottom[0]->count(), Dtype(1), temp);
        caffe_gpu_axpby(bottom[0]->count(), Dtype(1), temp, Dtype(0), bottom[0]->mutable_gpu_diff());
       // caffe_copy(bottom[0]->count(), temp, bottom[0]->mutable_cpu_diff());
    //compute loss
    caffe_gpu_add_scalar(bottom[0]->count(), Dtype(-1), temp);
    caffe_gpu_mul(bottom[0]->count(), P_data, temp_.gpu_data(), temp);
    Dtype loss;
    caffe_gpu_asum(bottom[0]->count(), temp_.gpu_data(), &loss);
    top[0]->mutable_cpu_data()[0] = loss/bottom[0]->num();
}

template <typename Dtype>
void KLLossLayer<Dtype>::Backward_gpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
    caffe_gpu_scale(bottom[0]->count(), top[0]->cpu_diff()[0]/bottom[0]->num(), bottom[0]->gpu_diff(), bottom[0]->mutable_gpu_diff());
    caffe_gpu_scale(bottom[1]->count(), top[0]->cpu_diff()[0]/bottom[0]->num(), bottom[1]->gpu_diff(), bottom[1]->mutable_gpu_diff());
  
}

INSTANTIATE_LAYER_GPU_FUNCS(KLLossLayer);

}  // namespace caffe
