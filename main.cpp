#include <iostream>
#include <vector>
#include <type_traits>
#include <algorithm>


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
  BufferTraits<double>::BCIterator bi_double;
};


template<typename O>
struct OBuffer
{
  OBufferIts::BCSizes bc_sizes;
  typename BufferTraits<int>::BCType bc_int;
  typename BufferTraits<double>::BCType bc_double;
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
  struct access<double, B>
  {
    static typename BufferTraits<double>::BCType& buffer(B &b)
    {
      return b.bc_double;
    }
    static const typename BufferTraits<double>::BCType& buffer(const B &b)
    {
      return b.bc_double;
    }
    static typename BufferTraits<double>::BCIterator& buffer_iterator(const B &b)
    {
      return b.its.bi_double;
    }
  };

  // specialized for container size buffer only, we don't allow for size_t as data type
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


template<typename T, typename B> inline
typename BufferTraits<T>::BCType& buffer(B &b)
{
  return traits::access<T, B>::buffer(b);
}


template<typename T, typename B> inline
const typename BufferTraits<T>::BCType& buffer(const B &b)
{
  return traits::access<T, B>::buffer(b);
}


template<typename T, typename B> inline
typename BufferTraits<T>::BCIterator& buffer_iterator(const B &b)
{
  return traits::access<T, B>::buffer_iterator(b);
}


template<typename B> inline
void operator << (B &b, int v)
{
  buffer<int>(b).push_back(v);
}

template<typename B> inline
void operator >> (const B &b, int &v)
{
  auto &it = buffer_iterator<int>(b);
  v = (*it);
  ++it;
}


template<typename B> inline
void operator << (B &b, double v)
{
  buffer<double>(b).push_back(v);
}


template<typename B> inline
  void operator >> (const B &b, double &v)
{
  auto &it = buffer_iterator<double>(b);
  v = (*it);
  ++it;
}


template<typename B, typename D1, typename D2> inline
void operator << (B &b, const std::pair<D1, D2> &c)
{
  b << c.first;
  b << c.second;
}


template<typename B, typename D1, typename D2> inline
void operator >> (const B &b, std::pair<D1, D2> &c)
{
  b >> c.first;
  b >> c.second;
}


template<typename B, typename D> inline
void operator << (B &b, const std::vector<D> &c)
{
  auto &bs = buffer<size_t>(b);
  bs.push_back(c.size());
  std::for_each(c.begin(), c.end(), [&b](const D& v){ b << v;});
}


template<typename B, typename D> inline
void operator >> (const B &b, std::vector<D> &c)
{  
  auto &bit_size = buffer_iterator<size_t>(b);
  const auto csize = *bit_size;
  ++bit_size;

  c.resize(csize);
  std::for_each(c.begin(), c.end(), [&b](D& v){ b >> v;});
}


template<typename B> inline
void init_load(const B& b)
{
  auto &bit_int = buffer_iterator<int>(b);
  bit_int = buffer<int>(b).begin(); 
  auto &bit_double = buffer_iterator<double>(b);
  bit_double = buffer<double>(b).begin(); 
  auto &bit_size = buffer_iterator<size_t>(b);
  bit_size = buffer<size_t>(b).begin(); 
}


/////////////////////////////////////////////////////
///                   USER                       ////
/////////////////////////////////////////////////////


struct SomeType
{
  std::vector<std::vector<int>> data;
  std::vector<int> more_data;
  std::pair<int,int> pair_data;
  std::vector<double> double_data;

};


template<typename Buffer> inline
void save(Buffer &b, const SomeType &d)
{
  b << d.data;
  b << d.more_data;
  b << d.pair_data;
  b << d.double_data;
}


template<typename Buffer> inline
void load(const Buffer &b, SomeType &d)
{
  b >> d.data;
  b >> d.more_data;
  b >> d.pair_data;
  b >> d.double_data;
}


int main()
{
  SomeType tput;
  tput.data.insert(tput.data.begin(), 10, std::vector<int>(5, 2));
  tput.more_data.insert(tput.more_data.begin(), 18, -3);
  tput.pair_data.first = 8;
  tput.pair_data.second = 3;
  tput.double_data.insert(tput.double_data.begin(), 12, 1.2);
  OBuffer<SomeType> b;
  save(b, tput);
  SomeType tget;
  init_load(b);
  load(b, tget);
  std::cout << std::boolalpha << (tget.data == tput.data) << "\n";
  std::cout << std::boolalpha << (tget.more_data == tput.more_data) << "\n";
  std::cout << std::boolalpha << (tget.pair_data == tput.pair_data) << "\n";
  std::cout << std::boolalpha << (tget.double_data == tput.double_data) << "\n";
  return 0;
}

