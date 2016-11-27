#include <iostream>
#include <vector>

template<typename T>
struct BufferTraits
{
  typedef std::vector<T> BType;
}; 

template<typename O>
struct OBuffer
{
  size_t o_count;
  typename BufferTraits<int>::BType b_int;
};


namespace traits
{
  template<typename T, typename B>
  struct access{};
  
  template<typename B>
  struct access<int, B>
  {
    static typename BufferTraits<int>::BType& buffer(B &b)
    {
      return b.b_int;
    }
  };
}

template<typename T, typename B>
typename BufferTraits<T>::BType& buffer(B &b)
{
  return traits::access<T,B>::buffer(b);
}

template<typename B, typename D>
B& operator << (B &b, const std::vector<D> &v)
{
  auto &bc = buffer<D>(b);
  bc.insert(bc.end(), v.begin(), v.end());
  return b;
}


/// users implement this
struct OtherType
{
  std::vector<int> data;
};

struct SomeType
{
  std::vector<int> data;
};

template<typename Buffer>
void serialize(Buffer &b, const SomeType &d)
{
  b << d.data;
}

template<typename Buffer>
void serialize(Buffer &b, const OtherType &d)
{
}

template<typename InputIt, typename OutputIt>
OutputIt mpi_dummy(InputIt first, InputIt last, OutputIt result)
{
  while(first != last)
    {
      first++; result++;
    }
  return result;
}
 
int main()
{
  SomeType t;
  OBuffer<SomeType> b;
  serialize(b, t);
  return 0;
}
