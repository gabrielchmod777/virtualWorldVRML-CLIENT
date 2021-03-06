#include "vrml_utils.h"
#include "avatar.h"
#include "client.h"

#include <string>
#include <algorithm>

#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>

avatar::avatar()
{
  // PRIVATE: can not use the default constructor; 
}

avatar::avatar(std::string name, client& associated_server_client, int avatar_gender)
{

  _avatar_gender = avatar_gender;
  _previousPosition = SbVec3f(0.0f, 0.0f, 0.0f);
  _name = name;
  _my_client = &associated_server_client;

  speed = 0;
  rotationSpeed = 0;
  orientation = 0;

  _3d_model = new SoSeparator();

  this->translation = new SoTranslation;
  this->_3d_model->addChild(this->translation);

  this->rotation = new SoRotation;
  this->_3d_model->addChild(this->rotation);

  if ( avatar_gender == 0 )
    {
      _3d_model->addChild(get_scene_graph_from_file("/usr/local/share/l3dclient/avatar_male.wrl"));
    }
  else
    {
      _3d_model->addChild(get_scene_graph_from_file("/usr/local/share/l3dclient/avatar_female.wrl"));
    }
  

  SoFont *myFont = new SoFont;
  SoSeparator* nameSeparator = new SoSeparator;
  SoTranslation* namePosition = new SoTranslation;
  SoText2* nameText = new SoText2;
  SoMaterial* nameColor = new SoMaterial;

  SoSeparator* handler = new SoSeparator;
  SoTranslation* handlerTranslation = new SoTranslation;
  handlerTranslation->translation.setValue(-0.5, 0, 0);
  SoSphere* handlerSph = new SoSphere;
  handlerSph->setName( name.c_str() );
  handlerSph->radius = 0.2;
  handler->addChild( handlerTranslation );
  handler->addChild( handlerSph );

  namePosition->translation.setValue(0, 6.5, 0);
  nameText->string = name.c_str();
  myFont->size.setValue( 15.0 );
  myFont->name.setValue( "TimesRoman" );
  nameColor->diffuseColor.setValue(1.0, 0.0, 0.0);
  nameSeparator->addChild( namePosition );
  nameSeparator->addChild( nameColor );
  nameSeparator->addChild( handler );
  nameSeparator->addChild( myFont );
  nameSeparator->addChild( nameText );
  
  _3d_model->addChild( nameSeparator );

  setPosition(0.0, 0.0, 0.0);
}

SoSeparator* avatar::get3d_model()
{
  return _3d_model;
}

void avatar::setSpeed(float v)
{ 
  speed=v; 
}

float avatar::getSpeed() const 
{ 
  return speed; 
}

SbVec3f avatar::getPosition() const 
{ 
  return translation->translation.getValue(); 
}

void avatar::getPosition(float &x, float &y, float &z) const 
{ 
  translation->translation.getValue().getValue(x, y, z); 
}

void avatar::setPosition(const SbVec3f &pozice)
{ 
  translation->translation.setValue(pozice); 
}

void avatar::setPosition(const float x, const float y, const float z) 
{ 
  translation->translation.setValue(x, y, z); 
}

float avatar::getOrientation() const 
{ 
  return orientation; 
}

void avatar::setOrientation(const float &angle)
{ 
  orientation=angle;
  rotation->rotation.setValue(SbVec3f(0, 1, 0), angle); 
}


void avatar::broadcastPosition()
{

  if( _previousPosition != getPosition() )
    {

      float x,y,z;
      getPosition(x,y,z);

      std::string xS = std::to_string(x); 
      std::string yS = std::to_string(y);
      std::string zS = std::to_string(z); 
      std::replace(xS.begin(), xS.end(), ',', '.');
      std::replace(yS.begin(), yS.end(), ',', '.');
      std::replace(zS.begin(), zS.end(), ',', '.');

      std::string cmd_move = " @js_eval_world var me = new avatar('"+_name+"','"+std::to_string(_avatar_gender)+"'); me.move( "+xS+" , "+yS+" , "+zS+" );";


      std::string oS = std::to_string( getOrientation() );
      std::replace(oS.begin(), oS.end(), ',', '.');

      std::string cmd_rotate = " @js_eval_world var me = new avatar('"+_name+"','"+std::to_string(_avatar_gender)+"'); me.rotate( 'Y' , "+oS+" )";  

      _my_client->send(std::move(cmd_move));
      _my_client->send(std::move(cmd_rotate));

    }

  _previousPosition = getPosition();
  
}

void avatar::chatWith(const std::string& other_name, const std::string& msg)
{
  std::string cmd_talk = " @js_eval_world var me = new avatar('"+_name+"','"+std::to_string(_avatar_gender)+"'); me.talk("+other_name+","+msg+")";  
  _my_client->send(std::move(cmd_talk));
}
