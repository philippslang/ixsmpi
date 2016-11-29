#include <iostream>
#include <vector>


template<typename T>
struct BufferTraits
{
  typedef std::vector<T> BCType;
  typedef typename BCType::const_iterator BCIterator;
}; 


struct OBufferIts
{
  typedef std::vector<size_t> BCSizes;
  typedef BCSizes::const_iterator BCSit;

  BCSit bi_sizes;
  BufferTraits<int>::BCIterator bi_int;
};


template<typename O>
struct OBuffer
{
  OBufferIts::BCSizes bc_sizes;
  typename BufferTraits<int>::BCType bc_int;
  mutable OBufferIts its;
};


namespace traits
{  
  template<typename T, typename B>
  struct access{};
  
  template<typename B>
  struct access<int, B>
  {
    static typename BufferTraits<int>::BCType& buffer(B &b)
    {
      return b.bc_int;
    }
    static const typename BufferTraits<int>::BCType& buffer(const B &b)
    {
      return b.bc_int;
    }
    static typename BufferTraits<int>::BCIterator& buffer_iterator(const B &b)
    {
      return b.its.bi_int;
    }
  };

  template<typename B>
  struct access<size_t, B>
  {
    static typename BufferTraits<size_t>::BCType& buffer(B &b)
    {
      return b.bc_sizes;
    }
    static const typename BufferTraits<size_t>::BCType& buffer(const B &b)
    {
      return b.bc_sizes;
    }
    static typename BufferTraits<size_t>::BCIterator& buffer_iterator(const B &b)
    {
      return b.its.bi_sizes;
    }
  };
}


template<typename T, typename B>
typename BufferTraits<T>::BCType& buffer(B &b)
{
  return traits::access<T, B>::buffer(b);
}


template<typename T, typename B>
const typename BufferTraits<T>::BCType& buffer(const B &b)
{
  return traits::access<T, B>::buffer(b);
}


template<typename T, typename B>
typename BufferTraits<T>::BCIterator& buffer_iterator(const B &b)
{
  return traits::access<T, B>::buffer_iterator(b);
}


template<typename B, typename D>
B& operator << (B &b, const std::vector<D> &c)
{
  auto &bc = buffer<D>(b);
  auto &bs = buffer<size_t>(b);
  bs.push_back(c.size());
  bc.insert(bc.end(), c.begin(), c.end());
  return b;
}


template<typename B, typename D>
const B& operator >> (const B &b, std::vector<D> &c)
{  
  auto &bit_size = buffer_iterator<size_t>(b);
  const auto csize = *bit_size;
  ++bit_size;

  c.resize(csize);
  auto &bit = buffer_iterator<D>(b);
  std::copy(bit, bit+csize, c.begin());
  return b;
}


template<typename B>
void init_load(const B& b)
{
  auto &bit_int = buffer_iterator<int>(b);
  bit_int = buffer<int>(b).begin(); 
  auto &bit_size = buffer_iterator<size_t>(b);
  bit_size = buffer<size_t>(b).begin(); 
}


/////////////////////////////////////////////////////
///                   USER                       ////
/////////////////////////////////////////////////////

struct SomeType
{
  std::vector<int> data;
};

template<typename Buffer>
void save(Buffer &b, const SomeType &d)
{
  b << d.data;
}

template<typename Buffer>
void load(const Buffer &b, SomeType &d)
{
  b >> d.data;
}


int main()
{
  SomeType tput;
  tput.data.insert(tput.data.begin(), 10, -1);
  OBuffer<SomeType> b;
  save(b, tput);
  SomeType tget;
  init_load(b);
  load(b, tget);
  return 0;
}



/*
template<typename InputIt, typename OutputIt>
OutputIt mpi_dummy(InputIt first, InputIt last, OutputIt result)
{
  while(first != last)
  {
    first++; result++;
  }
  return result;
}
 */