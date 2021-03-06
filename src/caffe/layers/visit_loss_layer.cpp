//bottom[0] should be 2*class type;
//speed up V.2, and new object (only constrain the path which doesn't pass through the within-class point.) , select a random j.
#include <vector>

#include "caffe/filler.hpp"
#include "caffe/layers/visit_loss_layer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe{
template <typename Dtype>
void VisitLossLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
     LossLayer<Dtype>::LayerSetUp(bottom, top);
}

template <typename Dtype>
void VisitLossLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
      vector<int> loss_shape(0);  // Loss layers output a scalar; 0 axes.
      top[0]->Reshape(1,1,1,1); 
      temp_.Reshape(bottom[0]->num()*bottom[0]->num(),bottom[0]->channels(),1,1);
      temp_data_.ReshapeLike(*bottom[0]);
      //log_data_.ReshapeLike(*bottom[0]);
      //for gpu codes
      sum_.Reshape(bottom[0]->num(), 1, 1, 1);
      max_.Reshape(bottom[0]->num(), 1, 1, 1);
      exp_.Reshape(bottom[0]->num(), bottom[0]->num(), 1, 1);
}

template <typename Dtype>
void VisitLossLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  const int N = bottom[0]->num();
  const int channels = bottom[0]->channels();
  const Dtype* bottom_data = bottom[0]->cpu_data();
  const Dtype* label = bottom[1]->cpu_data();
  Dtype* top_data = top[0]->mutable_cpu_data();
  Dtype* temp = temp_.mutable_cpu_data();
  
  
  top_data[0] = Dtype(0);
  
  caffe_set(bottom[0]->count(), Dtype(0), bottom[0]->mutable_cpu_diff());
  //compute exp value and sum value
  for(int i = 0; i < N; i++){
        for(int k = i; k < N; k++){
                if(i==k){
                        exp_.mutable_cpu_data()[i * N + k] = Dtype(0);//直接算出
                }
                else{
                        caffe_sub(channels, bottom_data + i * channels, bottom_data + k * channels, temp + (i * N + k) * channels);
                        caffe_cpu_scale(channels, Dtype(-1), temp + (i * N + k) * channels, temp + (k * N + i) * channels);
                        exp_.mutable_cpu_data()[i * N + k] = caffe_cpu_dot(channels, temp + (i * N + k) * channels, temp + (i * N + k) * channels) * Dtype(-0.5);//先算指数位置的值，为了求最大值
                        exp_.mutable_cpu_data()[k * N + i] = exp_.cpu_data()[i * N + k];
                }
        }
        Dtype max1=-100000;
        for(int j = 0; j < N; j++){
                if(i==j)continue;
                max1 = max1 >= exp_.cpu_data()[i * N + j] ? max1 : exp_.cpu_data()[i * N + j];
        }
        max_.mutable_cpu_data()[i] = max1;
  }
  for(int i = 0; i < N; i++){
        Dtype ans=0;
        for(int j = 0; j < N; j++){
                if(i!=j){
                        exp_.mutable_cpu_data()[i * N + j] = exp(exp_.cpu_data()[i * N + j] - max_.cpu_data()[i]);
                        ans += exp_.mutable_cpu_data()[i * N + j];
                }
        }
        sum_.mutable_cpu_data()[i] = ans;
  }
  
  for(int i = 0; i < N; i++){//sum i
        caffe_set(bottom[0]->count(), Dtype(0), temp_data_.mutable_cpu_diff());
        Dtype P = 0;
        
        //random j
        vector<int> J;
        for(int j = 0; j < N; j++)
                J.push_back(j);
        std::random_shuffle(J.begin(), J.end());
        
        for(int jj = 0; jj < 2; jj++){//sum j
                int j = J[jj];
                //exclude the within-class points;
                if(label[i]==label[j])continue;
                //compute the numerator of P1
                Dtype exp_ij = exp_.cpu_data()[i * N + j];
                Dtype P1 = exp_ij / sum_.cpu_data()[i];               
                //compute the numerator of P2
                Dtype exp_ji = exp_.cpu_data()[j * N + i];
                Dtype P2 = exp_ji / sum_.cpu_data()[j];
                
                P += P1 * P2; 
                
                //compute the gradients, the gradients are stored in temp_data_ 
                //for xi, speed up by integrating xk into xi and xj, resp.
                for(int k = 0; k < N; k++){
                        if(i==k)continue;
                        Dtype exp_ik = exp_.cpu_data()[i * N + k];
                        caffe_cpu_axpby(channels, P1*P2*exp_ik/sum_.cpu_data()[i], temp + (i * N + k) * channels, Dtype(1), temp_data_.mutable_cpu_diff() + i * channels);
                        //for xk
                        if(k!=i && k!=j){
                                caffe_cpu_axpby(channels, P1*P2*Dtype(-1)*exp_ik/sum_.cpu_data()[i], temp + (i * N + k) * channels, Dtype(1), temp_data_.mutable_cpu_diff() + k * channels);
                        }
                }
                caffe_cpu_axpby(channels, P1*P2*(Dtype(2)-P2), temp + (j * N + i) * channels, Dtype(1), temp_data_.mutable_cpu_diff() + i * channels);
                //for xj
                caffe_cpu_axpby(channels, Dtype(-1)*P1*P2*(Dtype(2)-P1), temp + (j * N + i) * channels, Dtype(1), temp_data_.mutable_cpu_diff() + j * channels);
                for(int k = 0; k < N; k++){
                        if(j==k)continue;
                        Dtype exp_jk = exp_.cpu_data()[j * N + k];
                        caffe_cpu_axpby(channels, P1*P2*exp_jk/sum_.cpu_data()[j], temp + (j * N + k) * channels, Dtype(1), temp_data_.mutable_cpu_diff() + j * channels);
                        //for xk
                        if(k!=i && k !=j){
                                caffe_cpu_axpby(channels, P1*P2*Dtype(-1)*exp_jk/sum_.cpu_data()[j], temp + (j * N + k) * channels, Dtype(1), temp_data_.mutable_cpu_diff() + k * channels);
                        }
                }

                
        }
        //sum(Plog(P)) for top_data[0]
        //top_data[0] += P * log(std::max(P,Dtype(1e-20))); 
        //sum(log(P))
        top_data[0] += log(std::max(P,Dtype(1e-20)));
        //sum P
        //top_data[0] += P;
        
        //scale the gradients for sum(Plog(p))
        //Dtype ans = log(std::max(P,Dtype(1e-20)))+Dtype(1);
        //sclae the gradients for sum(log(P))
        Dtype ans = Dtype(1)/std::max(Dtype(1e-20),P);
        //scale the gradients for sum(P)
        //Dtype ans = 1.;
        caffe_cpu_axpby(bottom[0]->count(), ans/Dtype(N), temp_data_.cpu_diff(), Dtype(1), bottom[0]->mutable_cpu_diff());
  }
  top_data[0] = top_data[0] / N;
}

template <typename Dtype>
void VisitLossLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down,
    const vector<Blob<Dtype>*>& bottom) {
        caffe_cpu_scale(bottom[0]->count(), top[0]->cpu_diff()[0], bottom[0]->cpu_diff(), bottom[0]->mutable_cpu_diff());
}
#ifdef CPU_ONLY
STUB_GPU(VisitLossLayer);
#endif

INSTANTIATE_CLASS(VisitLossLayer);
REGISTER_LAYER_CLASS(VisitLoss);

}

/*
//speed up V.2
#include <vector>

#include "caffe/filler.hpp"
#include "caffe/layers/visit_loss_layer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe{
template <typename Dtype>
void VisitLossLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
     LossLayer<Dtype>::LayerSetUp(bottom, top);
}

template <typename Dtype>
void VisitLossLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
      vector<int> loss_shape(0);  // Loss layers output a scalar; 0 axes.
      top[0]->Reshape(1,1,1,1); 
      temp_.Reshape(bottom[0]->num()*bottom[0]->num(),bottom[0]->channels(),1,1);
      temp_data_.ReshapeLike(*bottom[0]);
      //log_data_.ReshapeLike(*bottom[0]);
      //for gpu codes
      sum_.Reshape(bottom[0]->num(), 1, 1, 1);
      max_.Reshape(bottom[0]->num(), 1, 1, 1);
      exp_.Reshape(bottom[0]->num(), bottom[0]->num(), 1, 1);
}

template <typename Dtype>
void VisitLossLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  const int N = bottom[0]->num();
  const int channels = bottom[0]->channels();
  const Dtype* bottom_data = bottom[0]->cpu_data();
  Dtype* top_data = top[0]->mutable_cpu_data();
  Dtype* temp = temp_.mutable_cpu_data();
  
  
  top_data[0] = Dtype(0);
  
  caffe_set(bottom[0]->count(), Dtype(0), bottom[0]->mutable_cpu_diff());
  //compute exp value and sum value
  for(int i = 0; i < N; i++){
        for(int k = i; k < N; k++){
                if(i==k){
                        exp_.mutable_cpu_data()[i * N + k] = Dtype(0);//直接算出
                }
                else{
                        caffe_sub(channels, bottom_data + i * channels, bottom_data + k * channels, temp + (i * N + k) * channels);
                        caffe_cpu_scale(channels, Dtype(-1), temp + (i * N + k) * channels, temp + (k * N + i) * channels);
                        exp_.mutable_cpu_data()[i * N + k] = caffe_cpu_dot(channels, temp + (i * N + k) * channels, temp + (i * N + k) * channels) * Dtype(-0.5);//先算指数位置的值，为了求最大值
                        exp_.mutable_cpu_data()[k * N + i] = exp_.cpu_data()[i * N + k];
                }
        }
        Dtype max1=-100000;
        for(int j = 0; j < N; j++){
                if(i==j)continue;
                max1 = max1 >= exp_.cpu_data()[i * N + j] ? max1 : exp_.cpu_data()[i * N + j];
        }
        max_.mutable_cpu_data()[i] = max1;
  }
  for(int i = 0; i < N; i++){
        Dtype ans=0;
        for(int j = 0; j < N; j++){
                if(i!=j){
                        exp_.mutable_cpu_data()[i * N + j] = exp(exp_.cpu_data()[i * N + j] - max_.cpu_data()[i]);
                        ans += exp_.mutable_cpu_data()[i * N + j];
                }
        }
        sum_.mutable_cpu_data()[i] = ans;
  }
  
  for(int i = 0; i < N; i++){//sum i
        caffe_set(bottom[0]->count(), Dtype(0), temp_data_.mutable_cpu_diff());
        Dtype P = 0;
        
        for(int j = 0; j < N; j++){//sum j
                if(i==j)continue;
                //compute the numerator of P1
                Dtype exp_ij = exp_.cpu_data()[i * N + j];
                Dtype P1 = exp_ij / sum_.cpu_data()[i];               
                //compute the numerator of P2
                Dtype exp_ji = exp_.cpu_data()[j * N + i];
                Dtype P2 = exp_ji / sum_.cpu_data()[j];
                
                P += P1 * P2; 
                
                //compute the gradients, the gradients are stored in temp_data_ 
                //for xi, speed up by integrating xk into xi and xj, resp.
                for(int k = 0; k < N; k++){
                        if(i==k)continue;
                        Dtype exp_ik = exp_.cpu_data()[i * N + k];
                        caffe_cpu_axpby(channels, P1*P2*exp_ik/sum_.cpu_data()[i], temp + (i * N + k) * channels, Dtype(1), temp_data_.mutable_cpu_diff() + i * channels);
                        //for xk
                        if(k!=i && k!=j){
                                caffe_cpu_axpby(channels, P1*P2*Dtype(-1)*exp_ik/sum_.cpu_data()[i], temp + (i * N + k) * channels, Dtype(1), temp_data_.mutable_cpu_diff() + k * channels);
                        }
                }
                caffe_cpu_axpby(channels, P1*P2*(Dtype(2)-P2), temp + (j * N + i) * channels, Dtype(1), temp_data_.mutable_cpu_diff() + i * channels);
                //for xj
                caffe_cpu_axpby(channels, Dtype(-1)*P1*P2*(Dtype(2)-P1), temp + (j * N + i) * channels, Dtype(1), temp_data_.mutable_cpu_diff() + j * channels);
                for(int k = 0; k < N; k++){
                        if(j==k)continue;
                        Dtype exp_jk = exp_.cpu_data()[j * N + k];
                        caffe_cpu_axpby(channels, P1*P2*exp_jk/sum_.cpu_data()[j], temp + (j * N + k) * channels, Dtype(1), temp_data_.mutable_cpu_diff() + j * channels);
                        //for xk
                        if(k!=i && k !=j){
                                caffe_cpu_axpby(channels, P1*P2*Dtype(-1)*exp_jk/sum_.cpu_data()[j], temp + (j * N + k) * channels, Dtype(1), temp_data_.mutable_cpu_diff() + k * channels);
                        }
                }

                
        }
        //sum(Plog(P)) for top_data[0]
        //top_data[0] += P * log(std::max(P,Dtype(1e-20))); 
        //sum(log(P))
        //top_data[0] += log(std::max(P,Dtype(1e-20)));
        //sum P
        top_data[0] += P;
        
        //scale the gradients for sum(Plog(p))
        //Dtype ans = log(std::max(P,Dtype(1e-20)))+Dtype(1);
        //sclae the gradients for sum(log(P))
        //Dtype ans = Dtype(1)/std::max(Dtype(1e-20),P);
        //scale the gradients for sum(P)
        Dtype ans = 1.;
        caffe_cpu_axpby(bottom[0]->count(), ans/Dtype(N), temp_data_.cpu_diff(), Dtype(1), bottom[0]->mutable_cpu_diff());
  }
  top_data[0] = top_data[0] / N;
}

template <typename Dtype>
void VisitLossLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down,
    const vector<Blob<Dtype>*>& bottom) {
        caffe_cpu_scale(bottom[0]->count(), top[0]->cpu_diff()[0], bottom[0]->cpu_diff(), bottom[0]->mutable_cpu_diff());
}
#ifdef CPU_ONLY
STUB_GPU(VisitLossLayer);
#endif

INSTANTIATE_CLASS(VisitLossLayer);
REGISTER_LAYER_CLASS(VisitLoss);

}
*/


//speed up V.1
/*
#include <vector>

#include "caffe/filler.hpp"
#include "caffe/layers/visit_loss_layer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe{
template <typename Dtype>
void VisitLossLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
     LossLayer<Dtype>::LayerSetUp(bottom, top);
}

template <typename Dtype>
void VisitLossLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
      vector<int> loss_shape(0);  // Loss layers output a scalar; 0 axes.
      top[0]->Reshape(1,1,1,1); 
      temp_.Reshape(1,bottom[0]->channels(),1,1);
      temp_data_.ReshapeLike(*bottom[0]);
      //log_data_.ReshapeLike(*bottom[0]);
      //for gpu codes
      sum_.Reshape(bottom[0]->num(),1,1,1);
      exp_.Reshape(bottom[0]->num(), bottom[0]->num(), 1, 1);
}

template <typename Dtype>
void VisitLossLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  const int N = bottom[0]->num();
  const int channels = bottom[0]->channels();
  const Dtype* bottom_data = bottom[0]->cpu_data();
  Dtype* top_data = top[0]->mutable_cpu_data();
  Dtype* temp = temp_.mutable_cpu_data();
  
  
  top_data[0] = Dtype(0);
  
  caffe_set(bottom[0]->count(), Dtype(0), bottom[0]->mutable_cpu_diff());
  for(int i = 0; i < N; i++){//sum i
        caffe_set(bottom[0]->count(), Dtype(0), temp_data_.mutable_cpu_diff());
        Dtype P = 0;
        Dtype max_ik = -10000;
        vector<Dtype> sim_ik_vec;
        //find the max value when i for P1
        for(int k = 0; k < N; k++){//max k for P1
                if(i==k)continue;
                caffe_sub(channels, bottom_data + i * channels, bottom_data + k * channels, temp);
                Dtype sim_ik = caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                max_ik = max_ik >= sim_ik ? max_ik : sim_ik;
                sim_ik_vec.push_back(sim_ik);
        }
        //compute the sum value of k for P1
        Dtype sum_ik = 0;
        int cnt=0;
        for(int k = 0; k < N; k++){//sum k for P1
                if(i==k)continue;
                sum_ik += exp(sim_ik_vec[cnt] - max_ik);
                cnt++;
        }
        for(int j = 0; j < N; j++){//sum j
                if(i==j)continue;
                //compute the numerator of P1
                caffe_sub(channels, bottom_data + i * channels, bottom_data + j * channels, temp);
                Dtype sim_ij = caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                Dtype exp_ij = exp(sim_ij - max_ik);
                
                Dtype P1 = exp_ij / sum_ik;               
                //find the max value when j for P2
                Dtype max_jk = -10000;
                vector<Dtype> sim_jk_vec;
                for(int k = 0; k < N; k++){//max k for P2
                        if(j==k)continue;
                        caffe_sub(channels, bottom_data + j * channels, bottom_data + k * channels, temp);
                        Dtype sim_jk = caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                        max_jk = max_jk >= sim_jk ? max_jk : sim_jk;
                        sim_jk_vec.push_back(sim_jk);
                }
                //compute the sum value of k for P2
                Dtype sum_jk = 0;
                int cnt1=0;
                for(int k = 0; k < N; k++){//sum k for P2
                        if(j==k)continue;
                        sum_jk += exp(sim_jk_vec[cnt1] - max_jk);
                        cnt1++;
                }
                //compute the numerator of P2
                Dtype sim_ji = sim_ij;
                Dtype exp_ji = exp(sim_ji - max_jk);
                
                Dtype P2 = exp_ji / sum_jk;
                
                P += P1 * P2; 
                
                //compute the gradients, the gradients are stored in temp_data_ 
                //for xi, speed up by integrating xk into xi and xj, resp.
                for(int k = 0; k < N; k++){
                        if(i==k)continue;
                        caffe_sub(channels, bottom_data + i * channels, bottom_data + k * channels, temp);
                        Dtype sim_ik = caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                        Dtype exp_ik = exp(sim_ik - max_ik);
                        caffe_cpu_axpby(channels, P1*P2*exp_ik/sum_ik, temp, Dtype(1), temp_data_.mutable_cpu_diff() + i * channels);
                        //for xk
                        if(k!=i && k!=j){
                                caffe_cpu_axpby(channels, P1*P2*Dtype(-1)*exp_ik/sum_ik, temp, Dtype(1), temp_data_.mutable_cpu_diff() + k * channels);
                        }
                }
                caffe_sub(channels, bottom_data + j * channels, bottom_data + i * channels, temp);
                caffe_cpu_axpby(channels, P1*P2*(Dtype(2)-P2), temp, Dtype(1), temp_data_.mutable_cpu_diff() + i * channels);
                //for xj
                caffe_cpu_axpby(channels, Dtype(-1)*P1*P2*(Dtype(2)-P1), temp, Dtype(1), temp_data_.mutable_cpu_diff() + j * channels);
                for(int k = 0; k < N; k++){
                        if(j==k)continue;
                        caffe_sub(channels, bottom_data + j * channels, bottom_data + k * channels, temp);
                        Dtype sim_jk = caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                        Dtype exp_jk = exp(sim_jk - max_jk);
                        caffe_cpu_axpby(channels, P1*P2*exp_jk/sum_jk, temp, Dtype(1), temp_data_.mutable_cpu_diff() + j * channels);
                        //for xk
                        if(k!=i && k !=j){
                                caffe_cpu_axpby(channels, P1*P2*Dtype(-1)*exp_jk/sum_jk, temp, Dtype(1), temp_data_.mutable_cpu_diff() + k * channels);
                        }
                }
                
        }
        //sum(Plog(P)) for top_data[0]
        top_data[0] += P * log(std::max(P,Dtype(1e-20))); 
        
        
        //scale the gradients
        Dtype ans = log(std::max(P,Dtype(1e-20)))+Dtype(1);
        caffe_cpu_axpby(bottom[0]->count(), ans/Dtype(N), temp_data_.cpu_diff(), Dtype(1), bottom[0]->mutable_cpu_diff());
  }
  top_data[0] = top_data[0] / N;
}

template <typename Dtype>
void VisitLossLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down,
    const vector<Blob<Dtype>*>& bottom) {
        caffe_cpu_scale(bottom[0]->count(), top[0]->cpu_diff()[0], bottom[0]->cpu_diff(), bottom[0]->mutable_cpu_diff());
}
#ifdef CPU_ONLY
STUB_GPU(VisitLossLayer);
#endif

INSTANTIATE_CLASS(VisitLossLayer);
REGISTER_LAYER_CLASS(VisitLoss);

}
*/

//original version
/*
//bottom[0] should be 2*class type;
#include <vector>

#include "caffe/filler.hpp"
#include "caffe/layers/visit_loss_layer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe{
template <typename Dtype>
void VisitLossLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
     LossLayer<Dtype>::LayerSetUp(bottom, top);
}

template <typename Dtype>
void VisitLossLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
      vector<int> loss_shape(0);  // Loss layers output a scalar; 0 axes.
      top[0]->Reshape(1,1,1,1); 
      temp_.Reshape(1,bottom[0]->channels(),1,1);
      temp_data_.ReshapeLike(*bottom[0]);
      //log_data_.ReshapeLike(*bottom[0]);
}

template <typename Dtype>
void VisitLossLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  const int N = bottom[0]->num();
  const int channels = bottom[0]->channels();
  const Dtype* bottom_data = bottom[0]->cpu_data();
  Dtype* top_data = top[0]->mutable_cpu_data();
  Dtype* temp = temp_.mutable_cpu_data();
  
  
  top_data[0] = Dtype(0);
  
  caffe_set(bottom[0]->count(), Dtype(0), bottom[0]->mutable_cpu_diff());
  for(int i = 0; i < N; i++){//sum i
        caffe_set(bottom[0]->count(), Dtype(0), temp_data_.mutable_cpu_diff());
        Dtype P = 0;
        Dtype max_ik = -10000;
        //find the max value when i for P1
        for(int k = 0; k < N; k++){//max k for P1
                if(i==k)continue;
                caffe_sub(channels, bottom_data + i * channels, bottom_data + k * channels, temp);
                Dtype sim_ik = caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                max_ik = max_ik >= sim_ik ? max_ik : sim_ik;
        }
        //compute the sum value of k for P1
        Dtype sum_ik = 0;
        for(int k = 0; k < N; k++){//sum k for P1
                if(i==k)continue;
                caffe_sub(channels, bottom_data + i * channels, bottom_data + k * channels, temp);
                Dtype sim_ik =  caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                sum_ik += exp(sim_ik - max_ik);
        }
        for(int j = 0; j < N; j++){//sum j
                if(i==j)continue;
                //compute the numerator of P1
                caffe_sub(channels, bottom_data + i * channels, bottom_data + j * channels, temp);
                Dtype sim_ij = caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                Dtype exp_ij = exp(sim_ij - max_ik);
                
                Dtype P1 = exp_ij / sum_ik;               
                //find the max value when j for P2
                Dtype max_jk = -10000;
                for(int k = 0; k < N; k++){//max k for P2
                        if(j==k)continue;
                        caffe_sub(channels, bottom_data + j * channels, bottom_data + k * channels, temp);
                        Dtype sim_jk = caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                        max_jk = max_jk >= sim_jk ? max_jk : sim_jk;
                }
                //compute the sum value of k for P2
                Dtype sum_jk = 0;
                for(int k = 0; k < N; k++){//sum k for P2
                        if(j==k)continue;
                        caffe_sub(channels, bottom_data + j * channels, bottom_data + k * channels, temp);
                        Dtype sim_jk = caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                        sum_jk += exp(sim_jk - max_jk);
                }
                //compute the numerator of P2
                caffe_sub(channels, bottom_data + j * channels, bottom_data + i * channels, temp);
                Dtype sim_ji = caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                Dtype exp_ji = exp(sim_ji - max_jk);
                
                Dtype P2 = exp_ji / sum_jk;
                
                P += P1 * P2; 
                
                //compute the gradients, the gradients are stored in temp_data_ 
                //for xi
                for(int k = 0; k < N; k++){
                        if(i==k)continue;
                        caffe_sub(channels, bottom_data + i * channels, bottom_data + k * channels, temp);
                        Dtype sim_ik = caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                        Dtype exp_ik = exp(sim_ik - max_ik);
                        caffe_cpu_axpby(channels, P1*P2*exp_ik/sum_ik, temp, Dtype(1), temp_data_.mutable_cpu_diff() + i * channels);
                }
                caffe_sub(channels, bottom_data + j * channels, bottom_data + i * channels, temp);
                caffe_cpu_axpby(channels, P1*P2*(Dtype(2)-P2), temp, Dtype(1), temp_data_.mutable_cpu_diff() + i * channels);
                //for xj
                caffe_cpu_axpby(channels, Dtype(-1)*P1*P2*(Dtype(2)-P1), temp, Dtype(1), temp_data_.mutable_cpu_diff() + j * channels);
                for(int k = 0; k < N; k++){
                        if(j==k)continue;
                        caffe_sub(channels, bottom_data + j * channels, bottom_data + k * channels, temp);
                        Dtype sim_jk = caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                        Dtype exp_jk = exp(sim_jk - max_jk);
                        caffe_cpu_axpby(channels, P1*P2*exp_jk/sum_jk, temp, Dtype(1), temp_data_.mutable_cpu_diff() + j * channels);
                }
                //for xk
                for(int k = 0; k < N; k++){
                        if(k==i || k==j)continue;
                        caffe_sub(channels, bottom_data + i * channels, bottom_data + k * channels, temp);
                        Dtype sim_ik = caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                        Dtype exp_ik = exp(sim_ik - max_ik);
                        caffe_cpu_axpby(channels, P1*P2*Dtype(-1)*exp_ik/sum_ik, temp, Dtype(1), temp_data_.mutable_cpu_diff() + k * channels);
                        
                        caffe_sub(channels, bottom_data + j * channels, bottom_data + k * channels, temp);
                        Dtype sim_jk = caffe_cpu_dot(channels, temp, temp) * Dtype(-0.5);
                        Dtype exp_jk = exp(sim_jk - max_jk);
                        caffe_cpu_axpby(channels, P1*P2*Dtype(-1)*exp_jk/sum_jk, temp, Dtype(1), temp_data_.mutable_cpu_diff() + k * channels);
                }
       
                
        }
        //sum(Plog(P)) for top_data[0]
        top_data[0] += P * log(std::max(P,Dtype(1e-20))); 
        
        
        //scale the gradients
        Dtype ans = log(std::max(P,Dtype(1e-20)))+Dtype(1);
        caffe_cpu_axpby(bottom[0]->count(), ans/Dtype(N), temp_data_.cpu_diff(), Dtype(1), bottom[0]->mutable_cpu_diff());
  }
  top_data[0] = top_data[0] / N;
}

template <typename Dtype>
void VisitLossLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down,
    const vector<Blob<Dtype>*>& bottom) {
        caffe_cpu_scale(bottom[0]->count(), top[0]->cpu_diff()[0], bottom[0]->cpu_diff(), bottom[0]->mutable_cpu_diff());
}
#ifdef CPU_ONLY
STUB_GPU(VisitLossLayer);
#endif

INSTANTIATE_CLASS(VisitLossLayer);
REGISTER_LAYER_CLASS(VisitLoss);

}
*/
