/* for LSTM layer use, re-order the T x N x d input data at the T axis(axis-0). bottom_data[0] is input data, bottom_data[1] should the corresponding softmax scores, bottom_data[2] is the label;
if bottom input num is 1, it is reranked by the similarity_matrix computed by the input data at T axis.
*/
#include <algorithm>
#include <vector>
#include <cmath>

#include "caffe/layers/sequence_rerank_by_softmax.hpp"
#include "caffe/util/math_functions.hpp"


namespace caffe {

bool compare_ascending(std::pair<float, int> a, std::pair<float, int> b){

                return a.first<b.first;
}

bool compare_descending(std::pair<float, int> a, std::pair<float, int> b){

                return a.first>b.first;
}

template <typename Dtype>
void SequenceRerankBySoftmaxLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
    if(bottom.size()>1){
        CHECK_EQ(bottom[0]->count(0,2), bottom[1]->count(0,2)) << "The input-shape should be the same";
    }
}

template <typename Dtype>
void SequenceRerankBySoftmaxLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {

  // Initialize with the first blob.
  top[0]->ReshapeLike(*bottom[0]);
}

template <typename Dtype>
void SequenceRerankBySoftmaxLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
    
  if(bottom.size()>1){
        const Dtype* bottom_data = bottom[0]->cpu_data(); //T x N x d
        const Dtype* bottom_score = bottom[1]->cpu_data();//T x N x c
        const Dtype* bottom_label = bottom[2]->cpu_data();//T x N
        Dtype* top_data = top[0]->mutable_cpu_data();
  
        int T = bottom[0]->count(0,1);
        int N = bottom[0]->count(1,2);
        int d = bottom[0]->count(2);
        int class_num = bottom[1]->count(2);
  
        const bool ascending = this->layer_param_.sequence_rerank_by_softmax_param().ascending();
        for (int n = 0; n < N; n++){
                std::vector<std::pair<float, int> > score;
                //fill score vector
                for(int t = 0; t < T; t++){
                        score.push_back(std::make_pair(bottom_score[t * N * class_num + n * class_num + static_cast<int>(bottom_label[t * N + n])], t));
                }
        
                if(ascending){
                        std::sort(score.begin(), score.end(), compare_ascending);
                } else{
                        std::sort(score.begin(), score.end(), compare_descending);
                }
        
                for(int t = 0; t < T; t++){
                        caffe_copy(d, bottom_data + score[t].second * N * d + n * d, top_data + t * N * d + n * d);
                }
        }
  } else{
        const Dtype* bottom_data = bottom[0]->cpu_data(); //T x N x d
        Dtype* top_data = top[0]->mutable_cpu_data();
        
        int T = bottom[0]->count(0,1);
        int N = bottom[0]->count(1,2);
        int d = bottom[0]->count(2);
        const bool ascending = this->layer_param_.sequence_rerank_by_softmax_param().ascending();
        for(int n = 0; n < N; n++){
                std::vector<std::pair<float, int> > score;
                for(int t = 0; t < T; t++){
                        Dtype vec_norm1 = std::sqrt(std::max(caffe_cpu_dot(d, bottom_data + t * N * d + n * d,  bottom_data + t * N * d + n * d), Dtype(1e-10)));
                        Dtype cnt = 0.0;
                        //compute the similarities with other time step input data
                        for(int tt = 0; tt < T; tt++){
                                if(tt!=t){
                                        Dtype vec_norm2 = std::sqrt(std::max(caffe_cpu_dot(d, bottom_data + tt * N * d + n * d,  bottom_data + tt * N * d + n * d), Dtype(1e-10)));
                                        cnt += caffe_cpu_dot(d, bottom_data + t * N * d + n * d, bottom_data + tt * N * d + n * d)/(vec_norm1*vec_norm2);
                                }
                        }
                        score.push_back(std::make_pair(cnt, t));
                }
                if(ascending){
                        std::sort(score.begin(), score.end(), compare_ascending);
                } else{
                        std::sort(score.begin(), score.end(), compare_descending);
                }
                
                for(int t = 0; t < T; t++){
                        caffe_copy(d, bottom_data + score[t].second * N * d + n * d, top_data + t * N * d + n * d);
                }
        }
  }

}

template <typename Dtype>
void SequenceRerankBySoftmaxLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down,
    const vector<Blob<Dtype>*>& bottom) {
    
  if (propagate_down[0]) {
      if(bottom.size()>1){
        const Dtype* bottom_score = bottom[1]->cpu_data();
        const Dtype* top_diff = top[0]->cpu_diff();
        const Dtype* bottom_label = bottom[2]->cpu_data();//T x N
        Dtype* bottom_diff = bottom[0]->mutable_cpu_diff();
        
        int T = bottom[0]->count(0,1);
        int N = bottom[0]->count(1,2);
        int d = bottom[0]->count(2);
        int class_num = bottom[1]->count(2);
        
        const bool ascending = this->layer_param_.sequence_rerank_by_softmax_param().ascending();
        for(int n = 0; n < N; n++){
                std::vector<std::pair<float, int> > score;
                //file score vector
                for(int t = 0; t < T; t++){
                        score.push_back(std::make_pair(bottom_score[t * N * class_num + n * class_num + static_cast<int>(bottom_label[t * N + n])], t));
                }
                if(ascending){
                        std::sort(score.begin(), score.end(), compare_ascending);
                } else{
                        std::sort(score.begin(), score.end(), compare_descending);
                }
                for(int t = 0; t < T; t++){
                        caffe_copy(d, top_diff + t * N * d + n * d, bottom_diff + score[t].second * N * d + n * d);
                }
        }
     } else{
        const Dtype* bottom_data = bottom[0]->cpu_data(); //T x N x d
        const Dtype* top_diff = top[0]->cpu_diff();
        Dtype* bottom_diff = bottom[0]->mutable_cpu_data();
        
        int T = bottom[0]->count(0,1);
        int N = bottom[0]->count(1,2);
        int d = bottom[0]->count(2);
        const bool ascending = this->layer_param_.sequence_rerank_by_softmax_param().ascending();
        for(int n = 0; n < N; n++){
                std::vector<std::pair<float, int> > score;
                for(int t = 0; t < T; t++){
                        Dtype vec_norm1 = std::sqrt(std::max(caffe_cpu_dot(d, bottom_data + t * N * d + n * d,  bottom_data + t * N * d + n * d), Dtype(1e-10)));
                        Dtype cnt = 0.0;
                        //compute the similarities with other time step input data
                        for(int tt = 0; tt < T; tt++){
                                if(tt!=t){
                                        Dtype vec_norm2 = std::sqrt(std::max(caffe_cpu_dot(d, bottom_data + tt * N * d + n * d,  bottom_data + tt * N * d + n * d), Dtype(1e-10)));
                                        cnt += caffe_cpu_dot(d, bottom_data + t * N * d + n * d, bottom_data + tt * N * d + n * d)/(vec_norm1*vec_norm2);
                                }
                        }
                        score.push_back(std::make_pair(cnt, t));
                }
                if(ascending){
                        std::sort(score.begin(), score.end(), compare_ascending);
                } else{
                        std::sort(score.begin(), score.end(), compare_descending);
                }
                
                for(int t = 0; t < T; t++){
                        caffe_copy(d, top_diff + t * N * d + n * d, bottom_diff + score[t].second * N * d + n * d);
                }
        }
     }
  }
}


#ifdef CPU_ONLY
STUB_GPU(SequenceRerankBySoftmaxLayer);
#endif

INSTANTIATE_CLASS(SequenceRerankBySoftmaxLayer);
REGISTER_LAYER_CLASS(SequenceRerankBySoftmax);
}  // namespace caffe
