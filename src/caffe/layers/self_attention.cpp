//每个通道都对比
#include <algorithm>
#include "stdio.h"
#include "caffe/layers/self_attention.hpp"
#include "caffe/util/io.hpp"
#include "caffe/util/math_functions.hpp"
#include <cmath>
namespace caffe {

template <typename Dtype>
void SelfAttentionLossLayer<Dtype>::LayerSetUp(
  const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
  LossLayer<Dtype>::LayerSetUp(bottom, top);

}
template <typename Dtype>
void SelfAttentionLossLayer<Dtype>::Reshape(
    const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
  vector<int> loss_shape(0);  // Loss layers output a scalar; 0 axes.
  temp_.Reshape(1,1,bottom.size(),bottom.size());
  top[0]->Reshape(loss_shape);
}

template <typename Dtype>
void SelfAttentionLossLayer<Dtype>::Forward_cpu(
    const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
    int num = bottom[0]->num();
    int channels = bottom[0]->channels();
    int inner_num = bottom[0]->width()*bottom[0]->height();
    int K = bottom.size();

    Dtype* temp = temp_.mutable_cpu_data();
    top[0]->mutable_cpu_data()[0] = Dtype(0);
    
    for(int i = 0; i < num; i++){//loop num
       for(int c = 0; c < channels; c++){
        for(int j = 0; j < K; j++){
                const Dtype* data_j = bottom[j]->cpu_data();
                for(int z=0; z< K; z++){
                        const Dtype* data_z = bottom[z]->cpu_data();
                        if(j==z){
                                //temp[j*K+z] = Dtype(0);
                                temp[j*K+z] =caffe_cpu_dot(inner_num, data_j + i * channels * inner_num + c*inner_num, data_z + i * channels*inner_num + c*inner_num) - Dtype(1);//对于 不开根号的
                                continue;
                        }
                     
                        temp[j * K + z] = caffe_cpu_dot(inner_num, data_j + i * channels * inner_num + c*inner_num, data_z + i * channels*inner_num + c*inner_num);
                }
        }
        top[0]->mutable_cpu_data()[0] += caffe_cpu_dot(temp_.count(), temp, temp);
    }
    }

    top[0]->mutable_cpu_data()[0] = top[0]->mutable_cpu_data()[0] * Dtype(0.5)/Dtype(num)/Dtype(K*K*channels);
}

     

template <typename Dtype>
void SelfAttentionLossLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
	const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) 
{
    int num = bottom[0]->num();
    int channels = bottom[0]->channels();
    int inner_num = bottom[0]->width()*bottom[0]->height();
    int K = bottom.size();
    
    for(int i = 0; i < num; i++){
      for(int c= 0; c< channels; c++){
        for(int j = 0; j<K;j++){
                for(int z = 0; z < K; z++){
                        if(j!=z){
                                Dtype ans = caffe_cpu_dot(inner_num, bottom[j]->cpu_data() + i * channels * inner_num + c*inner_num, bottom[z]->cpu_data() + i * channels*inner_num + c*inner_num);
                                caffe_cpu_axpby(inner_num, Dtype(1)*ans/Dtype(num)*top[0]->cpu_diff()[0]/Dtype(K*K*channels), bottom[j]->cpu_data() + i * channels * inner_num + c*inner_num, Dtype(1), bottom[z]->mutable_cpu_diff() + i * channels * inner_num + c*inner_num);
                                caffe_cpu_axpby(inner_num, Dtype(1)*ans/Dtype(num)*top[0]->cpu_diff()[0]/Dtype(K*K*channels), bottom[z]->cpu_data() + i * channels * inner_num + c*inner_num, Dtype(1), bottom[j]->mutable_cpu_diff() + i * channels * inner_num + c*inner_num);
                        }
                        else{//对于不开根号的
                                Dtype ans = caffe_cpu_dot(inner_num, bottom[j]->cpu_data() + i * channels * inner_num + c*inner_num, bottom[z]->cpu_data() + i * channels*inner_num + c*inner_num) - Dtype(1);
                                caffe_cpu_axpby(inner_num, Dtype(2)*ans/Dtype(num)*top[0]->cpu_diff()[0]/Dtype(K*K*channels), bottom[j]->cpu_data() + i * channels * inner_num + c*inner_num, Dtype(1), bottom[z]->mutable_cpu_diff() + i * channels * inner_num + c*inner_num);
                        }
                }
        }
    }
    }
    
}




#ifdef CPU_ONLY
STUB_GPU(SelfAttentionLossLayer);
#endif

INSTANTIATE_CLASS(SelfAttentionLossLayer);
REGISTER_LAYER_CLASS(SelfAttentionLoss);

}  // namespace caffe
