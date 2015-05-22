/*
 * Shapeworks license
 */

// qt
#include <QThread>

// vtk
#include <vtkPolyData.h>

#include <Data/MeshManager.h>

MeshManager::MeshManager(Preferences& prefs) : prefs_(prefs), meshCache_(prefs), meshGenerator_(prefs)
{
	this->thread_count_ = 0;
}

MeshManager::~MeshManager() {}

void MeshManager::setNeighborhoodSize( int size )
{
  this->neighborhoodSize_ = size;
  this->meshCache_.clear();
  this->meshGenerator_.setNeighborhoodSize( size );
}

void MeshManager::setSampleSpacing( double spacing )
{
  this->sampleSpacing_ = spacing;
  this->meshCache_.clear();
  this->meshGenerator_.setSampleSpacing( spacing );
}

void MeshManager::clear_cache() { 
	this->meshCache_.clear(); 
	this->meshGenerator_.updatePipeline();
}

void MeshManager::generateMesh( const vnl_vector<double>& shape )
{
  // check cache first
  if ( !this->meshCache_.getMesh( shape )
       && !this->workQueue_.isInside( shape ) )
  {
      this->workQueue_.push( shape );
	  //todo
	  QThread *thread = new QThread;
	  MeshWorker *worker = new MeshWorker(this->prefs_,shape,&this->workQueue_,&this->meshCache_);
	  worker->moveToThread(thread);
  	  connect(thread, SIGNAL(started()), worker, SLOT(process()));
  	  connect(worker, SIGNAL(result_ready()),  this, SLOT(handle_thread_complete()));
  	  connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
  	  connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
	  if (this->thread_count_ < this->prefs_.get_num_threads()) {
  		thread->start();
		this->thread_count_++;
	  } else 
		threads_.push_back(thread);
  }
}

vtkSmartPointer<vtkPolyData> MeshManager::getMesh( const vnl_vector<double>& shape )
{
  vtkSmartPointer<vtkPolyData> polyData;

  // check cache first
  if ( this->meshCache_.getMesh( shape ) )
  {
    polyData = this->meshCache_.getMesh( shape );
  }
  else
  {
	  if ( (prefs_.get_parallel_enabled()) && 
		  (this->prefs_.get_num_threads() > 0))
	  {
		 this->generateMesh(shape);
	  } else {
		 polyData = this->meshGenerator_.buildMesh( shape );
		 this->meshCache_.insertMesh( shape, polyData );
	  }
  }
  return polyData;
}

void MeshManager::handle_thread_complete() {
	this->thread_count_ --;
	while (!this->threads_.empty() && this->thread_count_ < this->prefs_.get_num_threads()) {
		QThread *thread = this->threads_.back();
		this->threads_.pop_back();
		thread->start();
		this->thread_count_++;
	}
}