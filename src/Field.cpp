#include <houio/Field.h>


namespace houio
{
    template<> const int Field<float>::data_type_ = 1;
    template<> const int Field<math::V3f>::data_type_ = 2;
    template<> const int Field<double>::data_type_ = 3;
    template<> const int Field<math::V3d>::data_type_ = 4;
}
