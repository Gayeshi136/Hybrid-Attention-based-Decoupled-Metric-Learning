/* for LSTM layer use, re-order the T x N x d input data at the T axis(axis-0). bottom_data[0] is input data, bottom_data[1] should the corresponding softmax scores, bottom_data[2] is the label;
if bottom input num is 1, it is reranked by the similarity_matrix computed by the input data at T axis.
*/
#ifndef CAFFE_SEQUENCE_RERANK_BY_SOFTMAX_LAYER_HPP_
#define CAFFE_SEQUENCE_RERANK_BY_SOFTMAX_LAYER_HPP_

#include <vector>

#include "caffe/blob.hpp"
#include "caffe/layer.hpp"
#include "caffe/proto/caffe.pb.h"


namespace caffe {

template <typename Dtype>
class SequenceRerankBySoftmaxLayer : public Layer<Dtype> {
 public:

  explicit SequenceRerankBySoftmaxLayer(const LayerParameter& param)
      : Layer<Dtype>(param) {}
  virtual void LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top);
  virtual void Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top);
  virtual inline const char* type() const { return "SequenceRerankBySoftmax"; }
  
  virtual inline int MinBottomBlobs() const { return 1; }
  virtual inline int ExactNumTopBlobs() const { return 1; }

 protected:

  virtual void Forward_cpu(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top);

  virtual void Backward_cpu(const vector<Blob<Dtype>*>& top,
      const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom);
};

}  // namespace caffe

#endif  // CAFFE_RELU_LAYER_HPP_
