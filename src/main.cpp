#include "vrml_utils.h"
#include "gui.h"
#include "avatar.h"
#include "message_queue.h"
#include "client.h"
#include "executor.h"
#include "globals.h"
#include "login_dialog.h"

#include <cstdlib>
#include <memory>
#include <iostream>
#include <unistd.h>
#include <thread>

#include <boost/asio.hpp> 
#include <boost/thread/thread.hpp>

#include <Inventor/Qt/SoQt.h>         
#include <Inventor/Qt/viewers/SoQtExaminerViewer.h>  
#include <Inventor/SoDB.h>         
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/sensors/SoOneShotSensor.h>
#include <Inventor/actions/SoAction.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/details/SoFaceDetail.h>
#include <Inventor/details/SoPointDetail.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/SoPath.h>

#include <QDebug>
#include <QApplication>
#include <QUrl>
#include <QtWebKit/QWebView>
#include <QSound>

const float maxSpeed = 35.0f;
const float minSpeed = -10.0f;
const float speedUp = 4.0f;
const float slowDown = 8.0f;
const float maxRotSpeed = 1.5;
const float minRotSpeed = -1.5;
const float rotSpeedUp = 3;   
const float rotSlowDown = 15; 

const float BCAST_POS_TIME_INTERVAL = 0.5;
float _g_timeSinceLast_position_bcast = 1;

struct KeyRec
{
  enum {
    UP = 0, DOWN, LEFT, RIGHT,
  };
  SoKeyboardEvent::Key key;
  bool isDown;
} keyboard[] = {
  { SoKeyboardEvent::UP_ARROW, false },   // array with key states
  { SoKeyboardEvent::DOWN_ARROW, false }, // usage: keyboard[KeyRec::DOWN].isDown
  { SoKeyboardEvent::LEFT_ARROW, false },
  { SoKeyboardEvent::RIGHT_ARROW, false },
};

void keyboardEvent_cb(void *userdata, SoEventCallback *node);
void simulationStep(void *data, SoSensor *sensor);

avatar* user_avatar = NULL;
SoPerspectiveCamera* camera;
SoSelection* selection = NULL;

// Forward Declarations
SoSeparator* startUpScene( SoNode* avatar );
void made_selection( void * userdata, SoPath * path );

int main(int argc, char **argv)
{
  
  // Prepare the environment
  int sys_return_val = 0;
  std::string cmd = "l3dclient_mklocal_dir.sh";
  sys_return_val = std::system(cmd.c_str());
  std::cout<<std::endl<<"SYSTEM -> "<<sys_return_val<<std::endl;

  try
    {

      std::string name = "NONAME";
      std::string ip = "localhost";
      std::string port = "8080";

      QApplication app(argc, argv);  
      // SoQt::init ... only after QApplication...
      SoQt::init(argv[0]);

      // Log IN
      login_dialog ldia;
      int dialogCode = ldia.exec();
      
      if( dialogCode  == QDialog::Rejected )
	{
	  return 0;
	}
      else
	{
	  name = ldia.lineEdit_name->text().toStdString();
	  ip = ldia.lineEdit_ip->text().toStdString();
	  port = ldia.lineEdit_port->text().toStdString();
	}
      // END LOG IN

      boost::asio::io_service io_service;
      boost::asio::ip::tcp::resolver resolver(io_service);
      boost::asio::ip::tcp::resolver::query query(ip.c_str(), port.c_str());
      boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

      client c(io_service, iterator);
      
      std::string executors_name = name.c_str();
      command_executor client_exec(executors_name);
      c.add_observer(&client_exec);
      global_client = &c;

      qDebug()<<"==============================\n";
      // DOWNLOAD WORLD RESOURCES
      c.send(std::string("@get_resources"));
      qDebug()<<"==============================\n";
      
      int avatar_gender = ldia.selected_avatar;

      avatar my_avatar(executors_name, c, avatar_gender );
      user_avatar = & my_avatar;

      //start BOOST_ASIO
      boost::thread asio_th(boost::bind(&boost::asio::io_service::run, &io_service));

      if(asio_th.joinable()) {
        cout << "Detaching thread" << endl;
        asio_th.detach();
      }

      // The scene
      SoSeparator *root = startUpScene( my_avatar.get3d_model() );
      root->ref();

      gui viewer(root, app, camera);
      client_exec.set_vrml_tree_node( root );

      viewer.showMaximized();

      return app.exec();

    }  
  catch(std::exception &e)
    {
      std::cout<<std::endl<<e.what();
      exit(EXIT_FAILURE);
	}
  catch(std::string msg)
    {
      std::cout<<std::endl<<msg;
      exit(EXIT_FAILURE);
    }

  return EXIT_SUCCESS;
  
}


void keyboardEvent_cb(void *userdata, SoEventCallback *node)
{
  const SoEvent *event = node->getEvent();

  // handling of array keyboard
  for(int i=0; i<sizeof(keyboard)/sizeof(KeyRec); ++i)
  {
    if(SoKeyboardEvent::isKeyPressEvent(event, keyboard[i].key))
      keyboard[i].isDown=true;

    if(SoKeyboardEvent::isKeyReleaseEvent(event, keyboard[i].key))
      keyboard[i].isDown=false;
  }

  // handle ESC
  if(SoKeyboardEvent::isKeyPressEvent(event, SoKeyboardEvent::ESCAPE)) 
    {
      SoQt::exitMainLoop();
    }

  node->setHandled();
}


void BCAST_POSITION( double deltaTime )
{
  if( !gui::free_camera )
    {
      if(_g_timeSinceLast_position_bcast >= BCAST_POS_TIME_INTERVAL)
	{
	  user_avatar->broadcastPosition();
	  _g_timeSinceLast_position_bcast = 0;
	}

      _g_timeSinceLast_position_bcast += deltaTime;
    }
}

void STOP_AT_BORDER( SbVec3f pos , float speed)
{
  float posX = 0.0, posY = 0.0, posZ = 0.0;
  pos.getValue( posX, posY, posZ );
  
    if (posX < -2999)
    {
       user_avatar->setSpeed(0);
    }
  else if (posX > 2999)
    {
       user_avatar->setSpeed(0);
    }
  else if (posZ < -2999)
    {
      user_avatar->setSpeed(0);
    }
  else if (posZ > 2999)
    {
      user_avatar->setSpeed(0);
    }
  else
    {
      user_avatar->setSpeed(speed);
    }

}

void updateScene(double currentTime, double deltaTime)
{

  float speed = user_avatar->getSpeed();  // speed of user_avatar
  float orientation = user_avatar->getOrientation();
  SbVec3f originalPosition = user_avatar->getPosition();  // save current user_avatar->position

  // UP KEY
  // user_avatar->speed: acceleration
  if(keyboard[KeyRec::UP].isDown)
  {
    if(speed < maxSpeed)
    {
      speed += (float)deltaTime*speedUp;
      if(speed>maxSpeed)
        speed = maxSpeed;
    }
  }else
  {
    if(speed > 0)
    {
      speed -= (float)deltaTime*slowDown;
      if(speed<0)
        speed = 0;
    }
  }

  // KEY DOWN
    // user_avatar->speed: braking
  if(keyboard[KeyRec::DOWN].isDown)
  {
    if(speed > minSpeed)
    {
      speed -= (float)deltaTime*speedUp;
      if(speed < minSpeed)
        speed = minSpeed;
    }
  }else
  {
    if(speed < 0)
    {
      speed += (float)deltaTime*slowDown;
      if(speed > 0)
        speed = 0;
    }
  }

  // KEY LEFT
  // rotation speed: turn left
  if(keyboard[KeyRec::LEFT].isDown)
  {
    user_avatar->rotationSpeed += (float)deltaTime*rotSpeedUp;
    if(user_avatar->rotationSpeed > maxRotSpeed)
      user_avatar->rotationSpeed = maxRotSpeed;
  }else
  {
    if(user_avatar->rotationSpeed > 0)
    {
      user_avatar->rotationSpeed -= (float)deltaTime*rotSlowDown;
      if(user_avatar->rotationSpeed < 0)
        user_avatar->rotationSpeed = 0;
    }
  }

  // KEY RIGHT
  // rotation speed: turn right
  if(keyboard[KeyRec::RIGHT].isDown)
  {
    user_avatar->rotationSpeed -= (float)deltaTime*rotSpeedUp;
    if(user_avatar->rotationSpeed < minRotSpeed)
      user_avatar->rotationSpeed = minRotSpeed;
  }else
  {
    if(user_avatar->rotationSpeed < 0)
    {
      user_avatar->rotationSpeed += (float)deltaTime*rotSlowDown;
      if(user_avatar->rotationSpeed > 0)
        user_avatar->rotationSpeed = 0;
    }
  }

  // user_avatar->orientation
  // note: negative values must be used when going back
  if(speed >= 0)
    orientation += (float)deltaTime*user_avatar->rotationSpeed;
  else
    orientation -= (float)deltaTime*user_avatar->rotationSpeed;

  // set new user_avatar->orientation
  user_avatar->setOrientation(orientation);


  // set new user_avatar->position
  float distance = speed*(float)deltaTime;
  SbVec3f pos = user_avatar->getPosition();
  SbVec3f distanceVec(distance*sinf(orientation), 0.f, distance*cosf(orientation));
  pos -= distanceVec;
  user_avatar->setPosition(pos);

  STOP_AT_BORDER( pos , speed );
  user_avatar->direction = user_avatar->getPosition() - originalPosition;
  BCAST_POSITION( deltaTime );


  if(!gui::free_camera)
    {

      // 3rd person camera
      float tilt = -.20f;          // camera tilt (in radians)
      float cameraDistance = 15;   // initial camera distance from the user_avatar
      float cameraHeight = 8.0f;  // camera height from the user_avatar

      SbRotation cameraOrientation, cameraTilt;
      cameraOrientation.setValue(SbVec3f(0, 1, 0), orientation);
      cameraTilt.setValue(SbVec3f(1, 0, 0), tilt);
      camera->orientation = cameraTilt*cameraOrientation;

      if(speed < 0)
	{
	  cameraDistance = 18.0f-speed*0.44f;  // camera distance increases when going back
	}

      SbVec3f cameraPosition(user_avatar->getPosition());
      cameraPosition += SbVec3f(cameraDistance*sinf(orientation),
				cameraHeight, cameraDistance*cosf(orientation));
      camera->position.setValue(cameraPosition);
    }

}

void simulationStep(void *data, SoSensor *sensor)
{
  // compute time from the last run of simulationStep
  static double ct = SbTime::getTimeOfDay().getValue(); // current time
  static double lt; // last time

  lt = ct;
  ct = SbTime::getTimeOfDay().getValue();
  double dt = ct - lt; // delta time

  // update scene
  updateScene(ct, dt);

  // reschedule sensor (it will make this function to be called again and again)
  sensor->schedule();
}


// Forward Declarations
SoSeparator* startUpScene(SoNode* avatar)
{

  //**************************************************************
  // ================  STARTING WORLD STRUCTURE ==================
  //                 Separator (root0)
  //                        |
  //                Selection Node (root)__ ----> selection_callback
  //                        |______________ ----> un_select_callback
  //                        |
  //                  Camera Node
  //                        |
  //                World Skeleton & Floor 
  //                 from external files
  //                        |
  //                 Avatar ( Separator )
  //                        |
  //     Event Callback Node (for keyboard) -->kbd_callback function
  //                        |
  //        One Shot sensor ( repeat keyboard status check )
  //   _____________________|_________________________________
  //   |                |               |                   |
  // Separator       Separator       Separator          Separator
  //(sciences area) (bioSciences)   (Arts Area)  (Unassigned area)
  //   |                |               |                   |
  // Translation     Translation     Translation         Translation
  //   
  //**************************************************************

  // callbacks for selecting objects 
  SoSelection* root = new SoSelection;
  selection = root;
  root->ref();

  root->addSelectionCallback( made_selection, (void *)1L );
  root->addDeselectionCallback( made_selection, (void *)0L );
  root->policy = SoSelection::TOGGLE;

  //CAMERA
  camera = new SoPerspectiveCamera();
  camera->nearDistance = 4;
  camera->farDistance = 4096;
  root->addChild( camera );

  root->addChild( get_scene_graph_from_file("/usr/local/share/l3dclient/world.wrl") );
  root->addChild( get_scene_graph_from_file("/usr/local/share/l3dclient/grass.wrl") );
  root->addChild( avatar );

  ////////// move camera and avatar with directional keys
     
  // keyboard handling
  SoEventCallback *cb = new SoEventCallback;
  cb->addEventCallback(SoKeyboardEvent::getClassTypeId(), keyboardEvent_cb, NULL);
  root->insertChild(cb, 0);

  // sensor for infinite processing of simulationStep
  SoOneShotSensor *sensor = new SoOneShotSensor(simulationStep, NULL);
  sensor->schedule();


  SoSeparator* scienceArea = new SoSeparator;
  SoTranslation* sciTransl = new SoTranslation;
  sciTransl->translation.setValue(150, 0 ,150 );
  scienceArea->setName(VRML_UTILS_SCIENCES_AREA_NAME);
  root->addChild( scienceArea );
  scienceArea->addChild( sciTransl );
  scienceArea->addChild( get_scene_graph_from_file("/usr/local/share/l3dclient/flag_sci.wrl") ); 

  SoSeparator* bioArea = new SoSeparator;
  SoTranslation* bioTransl = new SoTranslation;
  bioTransl->translation.setValue(-150, 0 ,150 );
  bioArea->setName(VRML_UTILS_BIO_AREA_NAME);
  root->addChild( bioArea );
  bioArea->addChild( bioTransl );
  bioArea->addChild( get_scene_graph_from_file("/usr/local/share/l3dclient/flag_bio.wrl") ); 

  SoSeparator* artsArea = new SoSeparator;
  SoTranslation* artsTransl = new SoTranslation;
  artsTransl->translation.setValue(150, 0 ,-150 );
  artsArea->setName(VRML_UTILS_ARTS_AREA_NAME);
  root->addChild( artsArea );
  artsArea->addChild( artsTransl );
  artsArea->addChild( get_scene_graph_from_file("/usr/local/share/l3dclient/flag_arts.wrl") ); 

  SoSeparator* freeArea = new SoSeparator;
  SoTranslation* freeTransl = new SoTranslation;
  freeTransl->translation.setValue(-150, 0 ,-150 );
  freeArea->setName(VRML_UTILS_FREE_AREA_NAME);
  root->addChild( freeArea );
  freeArea->addChild( freeTransl );
  freeArea->addChild( get_scene_graph_from_file("/usr/local/share/l3dclient/flag_free.wrl") ); 

  // load resources from server

  root->unrefNoDelete();

  return root;
}

void made_selection( void * userdata, SoPath * path )
{
  qDebug()<<"sel";
   static SbBool lock = FALSE;
  // Avoid processing recursive calls when we explicitly
  // select/deselect paths in the toplevel SoSelection node.
  if ( lock ) return;
  lock = TRUE;

  selection->deselectAll();

  if ( userdata != NULL ) { // marks selection

    QString type = path->getTail()->getTypeId().getName().getString();
    qDebug()<<type<<"   "<<path->getTail()->getName().getString();

  }

  lock = FALSE;
  selection->touch();


}






/*
// the experiments and lessons have their own UI
// coded in HTML and JavaScript
// loaded in a QWebView browser (dynamic_web_ui)
//QWebView* dynamic_web_ui;
      ////////// end

      // dynamic_web_ui = new QWebView();
      // QString htmlSt = "<html><body><h1>HTML Previewer</h1>"
      //                 " <p>This example shows you how to use QWebView to"
      //                 " preview HTML data written in a QPlainTextEdit.</p>"
      //                 " </body></html>";
      // dynamic_web_ui->setHtml(htmlSt);
      // dynamic_web_ui->show();
      */
