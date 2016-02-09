/*
 * Copyright (c) 2015, Georgia Tech Research Corporation
 * All rights reserved.
 *
 * Author(s): Michael X. Grey <mxgrey@gatech.edu>
 *
 * Georgia Tech Graphics Lab and Humanoid Robotics Lab
 *
 * Directed by Prof. C. Karen Liu and Prof. Mike Stilman
 * <karenliu@cc.gatech.edu> <mstilman@cc.gatech.edu>
 *
 * This file is provided under the following "BSD-style" License:
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 */

#include <osg/Geode>
#include <osg/Geometry>

#include "kido/gui/osg/render/SoftMeshShapeNode.hpp"
#include "kido/gui/osg/Utils.hpp"

#include "kido/dynamics/SoftMeshShape.hpp"
#include "kido/dynamics/SoftBodyNode.hpp"
#include "kido/dynamics/PointMass.hpp"

namespace kido {
namespace gui {
namespace osg {
namespace render {

class SoftMeshShapeGeode : public ShapeNode, public ::osg::Geode
{
public:

  SoftMeshShapeGeode(kido::dynamics::SoftMeshShape* shape,
                     EntityNode* parentEntity,
                     SoftMeshShapeNode* parentNode);

  void refresh();
  void extractData();

protected:

  virtual ~SoftMeshShapeGeode();

  kido::dynamics::SoftMeshShape* mSoftMeshShape;
  SoftMeshShapeDrawable* mDrawable;

};

//==============================================================================
class SoftMeshShapeDrawable : public ::osg::Geometry
{
public:

  SoftMeshShapeDrawable(kido::dynamics::SoftMeshShape* shape);

  void refresh(bool firstTime);

protected:

  virtual ~SoftMeshShapeDrawable();

  ::osg::ref_ptr<::osg::Vec3Array> mVertices;
  ::osg::ref_ptr<::osg::Vec3Array> mNormals;
  ::osg::ref_ptr<::osg::Vec4Array> mColors;

  std::vector<Eigen::Vector3d> mEigNormals;

  kido::dynamics::SoftMeshShape* mSoftMeshShape;

};

//==============================================================================
SoftMeshShapeNode::SoftMeshShapeNode(
    std::shared_ptr<kido::dynamics::SoftMeshShape> shape,
    EntityNode* parent)
  : ShapeNode(shape, parent, this),
    mSoftMeshShape(shape),
    mGeode(nullptr)
{
  extractData(true);
  setNodeMask(mShape->isHidden()? 0x0 : ~0x0);
}

//==============================================================================
void SoftMeshShapeNode::refresh()
{
  mUtilized = true;

  setNodeMask(mShape->isHidden()? 0x0 : ~0x0);

  if(mShape->getDataVariance() == kido::dynamics::Shape::STATIC)
    return;

  extractData(false);
}

//==============================================================================
void SoftMeshShapeNode::extractData(bool firstTime)
{
  if(mShape->checkDataVariance(kido::dynamics::Shape::DYNAMIC_TRANSFORM)
     || firstTime)
    setMatrix(eigToOsgMatrix(mShape->getLocalTransform()));

  if(nullptr == mGeode)
  {
    mGeode = new SoftMeshShapeGeode(mSoftMeshShape.get(), mParentEntity, this);
    addChild(mGeode);
    return;
  }

  mGeode->refresh();
}

//==============================================================================
SoftMeshShapeNode::~SoftMeshShapeNode()
{
  // Do nothing
}

//==============================================================================
SoftMeshShapeGeode::SoftMeshShapeGeode(
    kido::dynamics::SoftMeshShape* shape,
    EntityNode* parentEntity,
    SoftMeshShapeNode* parentNode)
  : ShapeNode(parentNode->getShape(), parentEntity, this),
    mSoftMeshShape(shape),
    mDrawable(nullptr)
{
  getOrCreateStateSet()->setMode(GL_BLEND, ::osg::StateAttribute::ON);
  getOrCreateStateSet()->setRenderingHint(::osg::StateSet::TRANSPARENT_BIN);
  extractData();
}

//==============================================================================
void SoftMeshShapeGeode::refresh()
{
  mUtilized = true;

  extractData();
}

//==============================================================================
void SoftMeshShapeGeode::extractData()
{
  if(nullptr == mDrawable)
  {
    mDrawable = new SoftMeshShapeDrawable(mSoftMeshShape);
    addDrawable(mDrawable);
    return;
  }

  mDrawable->refresh(false);
}

//==============================================================================
SoftMeshShapeGeode::~SoftMeshShapeGeode()
{
  // Do nothing
}

//==============================================================================
SoftMeshShapeDrawable::SoftMeshShapeDrawable(
    kido::dynamics::SoftMeshShape* shape)
  : mVertices(new ::osg::Vec3Array),
    mNormals(new ::osg::Vec3Array),
    mColors(new ::osg::Vec4Array),
    mSoftMeshShape(shape)
{
  refresh(true);
}

static Eigen::Vector3d normalFromVertex(const kido::dynamics::SoftBodyNode* bn,
                                        const Eigen::Vector3i& face,
                                        size_t v)
{
  const Eigen::Vector3d& v0 = bn->getPointMass(face[v])->getLocalPosition();
  const Eigen::Vector3d& v1 = bn->getPointMass(face[(v+1)%3])->getLocalPosition();
  const Eigen::Vector3d& v2 = bn->getPointMass(face[(v+2)%3])->getLocalPosition();

  const Eigen::Vector3d dv1 = v1-v0;
  const Eigen::Vector3d dv2 = v2-v0;
  const Eigen::Vector3d n = dv1.cross(dv2);

  double weight = n.norm()/(dv1.norm()*dv2.norm());
  weight = std::max( -1.0, std::min( 1.0, weight) );

  return n.normalized() * asin(weight);
}

static void computeNormals(std::vector<Eigen::Vector3d>& normals,
                           const kido::dynamics::SoftBodyNode* bn)
{
  for(size_t i=0; i<normals.size(); ++i)
    normals[i] = Eigen::Vector3d::Zero();

  for(size_t i=0; i<bn->getNumFaces(); ++i)
  {
    const Eigen::Vector3i& face = bn->getFace(i);
    for(size_t j=0; j<3; ++j)
      normals[face[j]] += normalFromVertex(bn, face, j);
  }

  for(size_t i=0; i<normals.size(); ++i)
    normals[i].normalize();
}

//==============================================================================
void SoftMeshShapeDrawable::refresh(bool firstTime)
{
  if(mSoftMeshShape->getDataVariance() == kido::dynamics::Shape::STATIC)
    setDataVariance(::osg::Object::STATIC);
  else
    setDataVariance(::osg::Object::DYNAMIC);

  const kido::dynamics::SoftBodyNode* bn = mSoftMeshShape->getSoftBodyNode();

  if(mSoftMeshShape->checkDataVariance(kido::dynamics::Shape::DYNAMIC_ELEMENTS)
     || firstTime)
  {
    ::osg::ref_ptr<::osg::DrawElementsUInt> elements =
        new ::osg::DrawElementsUInt(GL_TRIANGLES);
    elements->reserve(3*bn->getNumFaces());

    for(size_t i=0; i < bn->getNumFaces(); ++i)
    {
      const Eigen::Vector3i& F = bn->getFace(i);
      for(size_t j=0; j<3; ++j)
        elements->push_back(F[j]);
    }

    addPrimitiveSet(elements);
  }

  if(   mSoftMeshShape->checkDataVariance(kido::dynamics::Shape::DYNAMIC_VERTICES)
     || mSoftMeshShape->checkDataVariance(kido::dynamics::Shape::DYNAMIC_ELEMENTS)
     || firstTime)
  {
    if(mVertices->size() != bn->getNumPointMasses())
      mVertices->resize(bn->getNumPointMasses());

    if(mNormals->size() != bn->getNumPointMasses())
      mNormals->resize(bn->getNumPointMasses());

    if(mEigNormals.size() != bn->getNumPointMasses())
      mEigNormals.resize(bn->getNumPointMasses());

    computeNormals(mEigNormals, bn);
    for(size_t i=0; i<bn->getNumPointMasses(); ++i)
    {
      (*mVertices)[i] = eigToOsgVec3(bn->getPointMass(i)->getLocalPosition());
      (*mNormals)[i] = eigToOsgVec3(mEigNormals[i]);
    }

    setVertexArray(mVertices);
    setNormalArray(mNormals, ::osg::Array::BIND_PER_VERTEX);
  }

  if(   mSoftMeshShape->checkDataVariance(kido::dynamics::Shape::DYNAMIC_COLOR)
     || firstTime)
  {
    if(mColors->size() != 1)
      mColors->resize(1);

    (*mColors)[0] = eigToOsgVec4(mSoftMeshShape->getRGBA());

    setColorArray(mColors, ::osg::Array::BIND_OVERALL);
  }
}

//==============================================================================
SoftMeshShapeDrawable::~SoftMeshShapeDrawable()
{
  // Do nothing
}

} // namespace render
} // namespace osg
} // namespace gui
} // namespace kido
