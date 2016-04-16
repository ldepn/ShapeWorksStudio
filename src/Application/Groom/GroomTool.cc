#include <iostream>

#include <QXmlStreamWriter>
#include <QTemporaryFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>

#include <Groom/GroomTool.h>
#include <Visualization/ShapeworksWorker.h>
#include <Data/Project.h>
#include <Data/Mesh.h>
#include <Data/Shape.h>

#include <ui_GroomTool.h>

//---------------------------------------------------------------------------
GroomTool::GroomTool(Preferences& prefs,std::vector<std::string>& files) 
  : preferences_(prefs), files_(files) {
  this->ui_ = new Ui_GroomTool;
  this->ui_->setupUi( this );
}

//---------------------------------------------------------------------------
GroomTool::~GroomTool()
{}

//---------------------------------------------------------------------------
void GroomTool::on_antialias_checkbox_stateChanged( int state )
{
  this->ui_->antialias_groupbox->setEnabled( state );
}

//---------------------------------------------------------------------------
void GroomTool::on_blur_checkbox_stateChanged( int state )
{
  this->ui_->blur_groupbox->setEnabled( state );
}


void GroomTool::on_fastmarching_checkbox_stateChanged(int state) {
  this->ui_->fastmarching_groupbox->setEnabled(state);
}

void GroomTool::on_autopad_checkbox_stateChanged(int state) {
  this->ui_->pad_groupbox->setEnabled(state);
}

//---------------------------------------------------------------------------
void GroomTool::on_export_xml_button_clicked()
{
  std::cerr << "Export XML\n";

  QString filename = QFileDialog::getSaveFileName( this, tr( "Save as..." ),
                                                   QString(), tr( "XML files (*.xml)" ) );
  if ( filename.isEmpty() )
  {
    return;
  }

  this->export_xml( filename );
}

//---------------------------------------------------------------------------
void GroomTool::handle_error(std::string msg) {
  emit error_message(msg);
}

//---------------------------------------------------------------------------
void GroomTool::handle_progress(int val) {
  emit progress(static_cast<size_t>(val));
}

void GroomTool::set_preferences() {
  this->ui_->center_checkbox->setChecked(
    this->preferences_.get_preference("groom_center", this->ui_->center_checkbox->isChecked()));
  this->ui_->antialias_checkbox->setChecked(
    this->preferences_.get_preference("groom_antialias", this->ui_->antialias_checkbox->isChecked()));
  this->ui_->autopad_checkbox->setChecked(
    this->preferences_.get_preference("groom_pad", this->ui_->autopad_checkbox->isChecked()));
  this->ui_->fastmarching_checkbox->setChecked(
    this->preferences_.get_preference("groom_fastmarching", this->ui_->fastmarching_checkbox->isChecked()));
  this->ui_->blur_checkbox->setChecked(
    this->preferences_.get_preference("groom_blur", this->ui_->blur_checkbox->isChecked()));
  this->ui_->autocrop_checkbox->setChecked(
    this->preferences_.get_preference("groom_crop", this->ui_->autocrop_checkbox->isChecked()));
  this->ui_->isolate_checkbox->setChecked(
    this->preferences_.get_preference("groom_isolate", this->ui_->isolate_checkbox->isChecked()));
  this->ui_->fill_holes_checkbox->setChecked(
  this->preferences_.get_preference("groom_fill_holes", this->ui_->fill_holes_checkbox->isChecked()));
  this->ui_->antialias_iterations->setValue(
    this->preferences_.get_preference("groom_antialias_amount", this->ui_->antialias_iterations->value()));
  this->ui_->iso_value->setValue(
    this->preferences_.get_preference("groom_fastmarching_isovalue", this->ui_->iso_value->value()));
  this->ui_->fastmarch_sigma->setValue(
    this->preferences_.get_preference("groom_fastmarching_sigma", this->ui_->fastmarch_sigma->value()));
  this->ui_->blur_sigma->setValue(
    this->preferences_.get_preference("groom_blur_sigma", this->ui_->blur_sigma->value()));
  this->ui_->padding_amount->setValue(
  this->preferences_.get_preference("groom_pad_value", this->ui_->padding_amount->value()));
}

void GroomTool::update_preferences() {
  this->preferences_.set_preference("groom_center", this->ui_->center_checkbox->isChecked());
  this->preferences_.set_preference("groom_antialias", this->ui_->antialias_checkbox->isChecked());
  this->preferences_.set_preference("groom_pad", this->ui_->autopad_checkbox->isChecked());
  this->preferences_.set_preference("groom_fastmarching", this->ui_->fastmarching_checkbox->isChecked());
  this->preferences_.set_preference("groom_blur", this->ui_->blur_checkbox->isChecked());
  this->preferences_.set_preference("groom_crop", this->ui_->autocrop_checkbox->isChecked());
  this->preferences_.set_preference("groom_isolate", this->ui_->isolate_checkbox->isChecked());
  this->preferences_.set_preference("groom_fill_holes", this->ui_->fill_holes_checkbox->isChecked());
  this->preferences_.set_preference("groom_antialias_amount", this->ui_->antialias_iterations->value());
  this->preferences_.set_preference("groom_fastmarching_isovalue", this->ui_->iso_value->value());
  this->preferences_.set_preference("groom_fastmarching_sigma", this->ui_->fastmarch_sigma->value());
  this->preferences_.set_preference("groom_blur_sigma", this->ui_->blur_sigma->value());
  this->preferences_.set_preference("groom_pad_value", this->ui_->padding_amount->value());
}

//---------------------------------------------------------------------------
void GroomTool::on_run_groom_button_clicked() {
  this->update_preferences();
  emit message("Please wait: running groom step...");
  emit progress(5);
  auto shapes = this->project_->get_shapes();
  std::vector<ImageType::Pointer> imgs;
  for (auto s : shapes) {
    imgs.push_back(s->get_original_image());
  }
  this->groom_ = new ShapeWorksGroom(this, imgs, 0, 1, 
    this->ui_->blur_sigma->value(),
    this->ui_->fastmarch_sigma->value(),
    this->ui_->iso_value->value(),
    this->ui_->padding_amount->value(), 
    this->ui_->antialias_iterations->value(), 
    true);

  emit progress(15);
  if ( this->ui_->center_checkbox->isChecked() ) {
    this->groom_->queueTool("center");
  }
  if ( this->ui_->autocrop_checkbox->isChecked() ) {
    this->groom_->queueTool("auto_crop");
  }
  if (this->ui_->fill_holes_checkbox->isChecked()) {
    this->groom_->queueTool("hole_fill");
  }
  if (this->ui_->autopad_checkbox->isChecked()) {
    this->groom_->queueTool("auto_pad");
  }
  if ( this->ui_->antialias_checkbox->isChecked() ) {
    this->groom_->queueTool("antialias");
  }
  if ( this->ui_->fastmarching_checkbox->isChecked() ) {   
    this->groom_->queueTool("fastmarching");
  }
  if ( this->ui_->blur_checkbox->isChecked() ) {
    this->groom_->queueTool("blur");
  }
  if (this->ui_->isolate_checkbox->isChecked()) {
    this->groom_->queueTool("isolate");
  }
  emit progress(10);
  this->ui_->run_groom_button->setEnabled(false);
  this->ui_->skipButton->setEnabled(false);
  QThread *thread = new QThread;
  ShapeworksWorker *worker = new ShapeworksWorker(
    ShapeworksWorker::Groom, this->groom_, nullptr, this->project_);
  worker->moveToThread(thread);
  connect(thread, SIGNAL(started()), worker, SLOT(process()));
  connect(worker, SIGNAL(result_ready()), this, SLOT(handle_thread_complete()));
  connect(this->groom_, SIGNAL(progress(int)), this, SLOT(handle_progress(int)));
  connect(worker, SIGNAL(error_message(std::string)), this, SLOT(handle_error(std::string)));
  connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
  thread->start();
}

//---------------------------------------------------------------------------
void GroomTool::handle_thread_complete() {
  emit progress(95);
  double iso = 0.5;
  if (this->groom_->tools().count("isolate") ||
    this->groom_->tools().count("hole_fill")) {
    iso = this->groom_->foreground() / 2.;
  }
  if (this->groom_->tools().count("fastmarching") ||
    this->groom_->tools().count("blur")) {
    iso = 0.;
  }
  this->project_->load_groomed_images(this->groom_->getImages(), iso);
  emit progress(100);
  emit message("Groom Complete");
  emit groom_complete();
  this->ui_->run_groom_button->setEnabled(true);
  this->ui_->skipButton->setEnabled(true);
}

//---------------------------------------------------------------------------
bool GroomTool::export_xml( QString filename )
{
  std::cerr << "export to " << filename.toStdString() << "\n";

  QFile file( filename );

  if ( !file.open( QIODevice::WriteOnly ) )
  {
    QMessageBox::warning( 0, "Read only", "The file is in read only mode" );
    return false;
  }

  QSharedPointer<QXmlStreamWriter> xml_writer = QSharedPointer<QXmlStreamWriter>( new QXmlStreamWriter() );
  xml_writer->setAutoFormatting( true );
  xml_writer->setDevice( &file );
  xml_writer->writeStartDocument();

  // background
  xml_writer->writeComment( "Value of background pixels in image" );
  xml_writer->writeTextElement( "background", "0.0" );

  // foreground
  xml_writer->writeComment( "Value of foreground pixels in image" );
  xml_writer->writeTextElement( "foreground", "1.0" );

  // pad
  /// TODO: hook up a UI element to control
  xml_writer->writeTextElement( "pad", QString::number( this->ui_->padding_amount->value() ) );

  QVector<QSharedPointer<Shape> > shapes = this->project_->get_shapes();

  QFileInfo fi( shapes[0]->get_original_filename_with_path() );
  QString project_path = fi.dir().absolutePath();

  // output transform
  xml_writer->writeTextElement( "transform_file", project_path + QDir::separator() + "studio.transform" );

  if ( this->ui_->antialias_checkbox->isChecked() )
  {
    xml_writer->writeComment( "Number of anti-aliasing iterations" );
    xml_writer->writeStartElement( "antialias_iterations" );
    xml_writer->writeCharacters( QString::number( this->ui_->antialias_iterations->value() ) );
    xml_writer->writeEndElement();
  }

  if ( this->ui_->blur_checkbox->isChecked() )
  {
    xml_writer->writeComment( "Size of Gaussian blurring kernel for smoothing" );
    xml_writer->writeStartElement( "blur_sigma" );
    xml_writer->writeCharacters( QString::number( this->ui_->blur_sigma->value() ) );
    xml_writer->writeEndElement();
  }

  // inputs
  xml_writer->writeStartElement( "inputs" );
  xml_writer->writeCharacters( "\n" );

  for ( int i = 0; i < shapes.size(); i++ )
  {
    xml_writer->writeCharacters( shapes[i]->get_original_filename_with_path() + "\n" );
  }
  xml_writer->writeEndElement();

  // outputs
  xml_writer->writeStartElement( "outputs" );
  xml_writer->writeCharacters( "\n" );
  shapes = this->project_->get_shapes();

  for ( int i = 0; i < shapes.size(); i++ )
  {
    QString path = shapes[i]->get_original_filename_with_path();

    QFileInfo fi( path );
    QString outfile = fi.dir().absolutePath() + QDir::separator() + fi.completeBaseName() + "_DT.nrrd";

    xml_writer->writeCharacters( outfile + "\n" );
  }
  xml_writer->writeEndElement();

  xml_writer->writeEndDocument();

  return true;
}

//---------------------------------------------------------------------------
void GroomTool::set_project( QSharedPointer<Project> project )
{
  this->project_ = project;
}

void GroomTool::on_skipButton_clicked() {
  this->update_preferences();
  auto shapes = this->project_->get_shapes();
  std::vector<ImageType::Pointer> imgs;
  for (auto s : shapes) {
    imgs.push_back(s->get_original_image());
  }
  this->project_->load_groomed_images(imgs, this->ui_->iso_value->value());
  emit message("Skipped groom.");
  emit groom_complete();
}