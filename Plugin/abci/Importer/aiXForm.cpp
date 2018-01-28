#include "pch.h"
#include "aiInternal.h"
#include "aiContext.h"
#include "aiObject.h"
#include "aiSchema.h"
#include "aiXForm.h"


aiXformSample::aiXformSample(aiXform *schema)
    : super(schema), inherits(true)
{
}

void aiXformSample::updateConfig(const aiConfig &config, bool &data_changed)
{
    data_changed = (config.swap_handedness != m_config.swap_handedness);
    m_config = config;
}

void aiXformSample::decompose(const Imath::M44d &mat,Imath::V3d &scale,Imath::V3d &shear,Imath::Quatd &rotation,Imath::V3d &translation) const
{
    Imath::M44d mat_remainder(mat);

    // Extract Scale, Shear
    Imath::extractAndRemoveScalingAndShear(mat_remainder, scale, shear);

    // Extract translation
    translation.x = mat_remainder[3][0];
    translation.y = mat_remainder[3][1];
    translation.z = mat_remainder[3][2];

    // Extract rotation
    rotation = extractQuat(mat_remainder);
}

void aiXformSample::getData(aiXformData &dst) const
{
    Imath::V3d scale;
    Imath::V3d shear;
    Imath::Quatd rot;
    Imath::V3d trans;
    decompose(m_matrix, scale, shear, rot, trans);

    if (m_config.interpolate_samples && m_current_time_offset!=0)
    {
        Imath::V3d scale2;
        Imath::Quatd rot2;
        Imath::V3d trans2;
        decompose(m_next_matrix, scale2, shear, rot2, trans2);
        scale += (scale2 - scale)* m_current_time_offset;
        trans += (trans2 - trans)* m_current_time_offset;
        rot = slerpShortestArc(rot, rot2, (double)m_current_time_offset);
    }

    auto rotFinal = abcV4(
        static_cast<float>(rot.v[0]),
        static_cast<float>(rot.v[1]),
        static_cast<float>(rot.v[2]),
        static_cast<float>(rot.r)
    );

    if (m_config.swap_handedness)
    {
        trans.x *= -1.0f;
        rotFinal.x = -rotFinal.x;
        rotFinal.w = -rotFinal.w;
    }
    dst.inherits = inherits;
    dst.translation = trans;
    dst.rotation = rotFinal;
    dst.scale = scale;
}

aiXform::aiXform(aiObject *obj)
    : super(obj)
{
}

aiXform::Sample* aiXform::newSample()
{
    Sample *sample = getSample();
    if (!sample)
        sample = new Sample(this);
    return sample;
}

aiXform::Sample* aiXform::readSample(const uint64_t idx)
{
    Sample *ret = newSample();

    auto ss = aiIndexToSampleSelector(idx);
    AbcGeom::XformSample matSample;
    m_schema.get(matSample, ss);
    ret->m_matrix = matSample.getMatrix();
    
    ret->inherits = matSample.getInheritsXforms();

    auto ss2 = aiIndexToSampleSelector(idx + 1);
    AbcGeom::XformSample nextMatSample;
    m_schema.get(nextMatSample, ss2 );
    ret->m_next_matrix = nextMatSample.getMatrix();
    
    return ret;
}