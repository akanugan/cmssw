#include "RecoLocalTracker/SiPixelRecHits/interface/PixelCPEGeneric.h"

#include "Geometry/TrackerGeometryBuilder/interface/PixelGeomDetUnit.h"
#include "Geometry/TrackerGeometryBuilder/interface/RectangularPixelTopology.h"

// this is needed to get errors from templates
#include "RecoLocalTracker/SiPixelRecHits/interface/SiPixelTemplate.h"
#include "DataFormats/SiPixelDetId/interface/PXBDetId.h"
#include "DataFormats/DetId/interface/DetId.h"


// Services
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "MagneticField/Engine/interface/MagneticField.h"

#include "boost/multi_array.hpp"

#include <iostream>
using namespace std;

namespace {
  constexpr float HALF_PI = 1.57079632679489656;
  constexpr float PI = 2*HALF_PI;
  const bool useNewSimplerErrors = false;
}

#define NEW 

//-----------------------------------------------------------------------------
//!  The constructor.
//-----------------------------------------------------------------------------
#ifdef NEW
PixelCPEGeneric::PixelCPEGeneric(edm::ParameterSet const & conf, 
				 const MagneticField * mag,
                                 const TrackerGeometry& geom,
				 const SiPixelLorentzAngle * lorentzAngle, 
				 const SiPixelGenErrorDBObject * genErrorDBObject, 
				 const SiPixelTemplateDBObject * templateDBobject,
				 const SiPixelLorentzAngle * lorentzAngleWidth=0) 
  : PixelCPEBase(conf, mag, geom, lorentzAngle, genErrorDBObject, templateDBobject,lorentzAngleWidth)
#else
PixelCPEGeneric::PixelCPEGeneric(edm::ParameterSet const & conf, 
				 const MagneticField * mag, 
                                 const TrackerGeometry& geom,
				 const SiPixelLorentzAngle * lorentzAngle, 
				 const SiPixelCPEGenericErrorParm * genErrorParm, 
				 const SiPixelTemplateDBObject * templateDBobject,
				 const SiPixelLorentzAngle * lorentzAngleWidth=0) 
  : PixelCPEBase(conf, mag, geom, lorentzAngle, genErrorParm, templateDBobject,lorentzAngleWidth)
#endif
{
  
  if (theVerboseLevel > 0) 
    LogDebug("PixelCPEGeneric") 
      << " constructing a generic algorithm for ideal pixel detector.\n"
      << " CPEGeneric:: VerboseLevel = " << theVerboseLevel;

  // Externally settable cuts  
  the_eff_charge_cut_lowX = conf.getParameter<double>("eff_charge_cut_lowX");
  the_eff_charge_cut_lowY = conf.getParameter<double>("eff_charge_cut_lowY");
  the_eff_charge_cut_highX = conf.getParameter<double>("eff_charge_cut_highX");
  the_eff_charge_cut_highY = conf.getParameter<double>("eff_charge_cut_highY");
  the_size_cutX = conf.getParameter<double>("size_cutX");
  the_size_cutY = conf.getParameter<double>("size_cutY");

  EdgeClusterErrorX_ = conf.getParameter<double>("EdgeClusterErrorX");
  EdgeClusterErrorY_ = conf.getParameter<double>("EdgeClusterErrorY");

  // Externally settable flags to inflate errors
  inflate_errors = conf.getParameter<bool>("inflate_errors");
  inflate_all_errors_no_trk_angle = conf.getParameter<bool>("inflate_all_errors_no_trk_angle");

  UseErrorsFromTemplates_    = conf.getParameter<bool>("UseErrorsFromTemplates");
  TruncatePixelCharge_       = conf.getParameter<bool>("TruncatePixelCharge");
  IrradiationBiasCorrection_ = conf.getParameter<bool>("IrradiationBiasCorrection");
  DoCosmics_                 = conf.getParameter<bool>("DoCosmics");
  LoadTemplatesFromDB_       = conf.getParameter<bool>("LoadTemplatesFromDB");

  // This LA related parameters are only relevant for the Generic algo

  //lAOffset_ = conf.getUntrackedParameter<double>("lAOffset",0.0);
  lAOffset_ = conf.existsAs<double>("lAOffset")?
              conf.getParameter<double>("lAOffset"):0.0;
  //lAWidthBPix_  = conf.getUntrackedParameter<double>("lAWidthBPix",0.0);
  lAWidthBPix_ = conf.existsAs<double>("lAWidthBPix")?
                 conf.getParameter<double>("lAWidthBPix"):0.0;
  //lAWidthFPix_  = conf.getUntrackedParameter<double>("lAWidthFPix",0.0);
  lAWidthFPix_ = conf.existsAs<double>("lAWidthFPix")?
                 conf.getParameter<double>("lAWidthFPix"):0.0;

  // Use LA-offset from config, for testing only
  if(lAOffset_>0.0) useLAOffsetFromConfig_ = true;
  // Use LA-width from config, split into fpix & bpix, for testing only
  if(lAWidthBPix_>0.0 || lAWidthFPix_>0.0) useLAWidthFromConfig_ = true;

  // Use LA-width from DB. If both (upper and this) are false LA-width is calcuated from LA-offset
  //useLAWidthFromDB_ = conf.getParameter<bool>("useLAWidthFromDB");
  useLAWidthFromDB_ = conf.existsAs<bool>("useLAWidthFromDB")?
    conf.getParameter<bool>("useLAWidthFromDB"):false;

  // Use Alignment LA-offset 
  useLAAlignmentOffsets_ = conf.existsAs<bool>("useLAAlignmentOffsets")?
    conf.getParameter<bool>("useLAAlignmentOffsets"):false;

  // Select the position error source 
  // For upgrde and cosmics force the use simple errors 
  if( isUpgrade_ || (!with_track_angle && DoCosmics_) ) UseErrorsFromTemplates_ = false;


  if ( !UseErrorsFromTemplates_ && ( TruncatePixelCharge_       || 
				     IrradiationBiasCorrection_ || 
				     DoCosmics_                 ||
				     LoadTemplatesFromDB_ ) )  {
    throw cms::Exception("PixelCPEGeneric::PixelCPEGeneric: ") 
      << "\nERROR: UseErrorsFromTemplates_ is set to False in PixelCPEGeneric_cfi.py. "
      << " In this case it does not make sense to set any of the following to True: " 
      << " TruncatePixelCharge_, IrradiationBiasCorrection_, DoCosmics_, LoadTemplatesFromDB_ !!!" 
      << "\n\n";
  }

  // Use errors from templates 
  if ( UseErrorsFromTemplates_ ) {

    if(useNewSimplerErrors) {  // use new errors from mini templates 

      gtemplID_ = -999;
      if ( LoadTemplatesFromDB_ )  {  // From DB
	if ( !gtempl_.pushfile( *genErrorDBObject_) )
          throw cms::Exception("InvalidCalibrationLoaded") 
            << "ERROR: Templates not filled correctly. Check the sqlite file. Using SiPixelTemplateDBObject version " 
            << ( *genErrorDBObject_ ).version() << ". GenError ID is " << gtemplID_;
      } else  { // From file      
        if ( !gtempl_.pushfile( gtemplID_ ) )
          throw cms::Exception("InvalidCalibrationLoaded") 
            << "ERROR: Templates not loaded correctly from text file. Reconstruction will fail." 
	    << " Template ID is " << gtemplID_;
      } // if load from DB

    } else {  // use old errors from full templates

      templID_ = -999;
      if ( LoadTemplatesFromDB_ ) {
	// Initialize template store to the selected ID [Morris, 6/25/08]  
	if ( !templ_.pushfile( *templateDBobject_) )
	  throw cms::Exception("InvalidCalibrationLoaded") 
	    << "ERROR: Templates not filled correctly. Check the sqlite file. Using SiPixelTemplateDBObject version " 
	    << ( *templateDBobject_ ).version() << ". Template ID is " << templID_;
      } else {
	if ( !templ_.pushfile( templID_ ) )
	  throw cms::Exception("InvalidCalibrationLoaded") 
	    << "ERROR: Templates not loaded correctly from text file. Reconstruction will fail." 
	    << " Template ID is " << templID_;
      } // if load from DB

    } // if useNewSimpleErrors 
  } // if ( UseErrorsFromTemplates_ )
  
  //cout << endl;
  //cout << "From PixelCPEGeneric::PixelCPEGeneric(...)" << endl;
  //cout << "(int)UseErrorsFromTemplates_ = " << (int)UseErrorsFromTemplates_    << endl;
  //cout << "TruncatePixelCharge_         = " << (int)TruncatePixelCharge_       << endl;      
  //cout << "IrradiationBiasCorrection_   = " << (int)IrradiationBiasCorrection_ << endl;
  //cout << "(int)DoCosmics_              = " << (int)DoCosmics_                 << endl;
  //cout << "(int)LoadTemplatesFromDB_    = " << (int)LoadTemplatesFromDB_       << endl;
  //cout << endl;


  // Default case for rechit errors in case other, more correct, errors are not vailable
  // This are constants. Maybe there is a more efficienct way to store them.
  xerr_barrel_l1_= {0.00115, 0.00120, 0.00088};
  xerr_barrel_l1_def_=0.01030;
  yerr_barrel_l1_= {0.00375,0.00230,0.00250,0.00250,0.00230,0.00230,0.00210,0.00210,0.00240};
  yerr_barrel_l1_def_=0.00210;
  xerr_barrel_ln_= {0.00115, 0.00120, 0.00088};
  xerr_barrel_ln_def_=0.01030;
  yerr_barrel_ln_= {0.00375,0.00230,0.00250,0.00250,0.00230,0.00230,0.00210,0.00210,0.00240};
  yerr_barrel_ln_def_=0.00210;
  xerr_endcap_= {0.0020, 0.0020};
  xerr_endcap_def_=0.0020;
  yerr_endcap_= {0.00210};
  yerr_endcap_def_=0.00075;

  bool isUpgrade=false;
  if ( conf.exists("Upgrade") && conf.getParameter<bool>("Upgrade")) {
    isUpgrade=true;
    xerr_barrel_ln_= {0.00114,0.00104,0.00214};
    xerr_barrel_ln_def_=0.00425;
    yerr_barrel_ln_= {0.00299,0.00203,0.0023,0.00237,0.00233,0.00243,0.00232,0.00259,0.00176};
    yerr_barrel_ln_def_=0.00245;
    xerr_endcap_= {0.00151,0.000813,0.00221};
    xerr_endcap_def_=0.00218;
    yerr_endcap_= {0.00261,0.00107,0.00264};
    yerr_endcap_def_=0.00357;
    
    if ( conf.exists("SmallPitch") && conf.getParameter<bool>("SmallPitch")) {
      xerr_barrel_l1_= {0.00104, 0.000691, 0.00122};
      xerr_barrel_l1_def_=0.00321;
      yerr_barrel_l1_= {0.00199,0.00136,0.0015,0.00153,0.00152,0.00171,0.00154,0.00157,0.00154};
      yerr_barrel_l1_def_=0.00164;
    }
    else{
      xerr_barrel_l1_= {0.00114,0.00104,0.00214};
      xerr_barrel_l1_def_=0.00425;
      yerr_barrel_l1_= {0.00299,0.00203,0.0023,0.00237,0.00233,0.00243,0.00232,0.00259,0.00176};
      yerr_barrel_l1_def_=0.00245;
    }
  }
  isUpgrade_=isUpgrade;

}



//-----------------------------------------------------------------------------
//! Hit position in the local frame (in cm).  Unlike other CPE's, this
//! one converts everything from the measurement frame (in channel numbers) 
//! into the local frame (in centimeters).  
//-----------------------------------------------------------------------------
LocalPoint
PixelCPEGeneric::localPosition(const SiPixelCluster& cluster) const 
{

  //cout<<" in PixelCPEGeneric:localPosition - "<<endl; //dk

  computeLorentzShifts();  //!< correctly compute lorentz shifts in X and Y

  float chargeWidthX = (lorentzShiftInCmX_ * theParam->widthLAFraction);
  float chargeWidthY = (lorentzShiftInCmY_ * theParam->widthLAFraction);
  float shiftX = 0.5f*lorentzShiftInCmX_;
  float shiftY = 0.5f*lorentzShiftInCmY_;

  if ( UseErrorsFromTemplates_ ) {
      const float micronsToCm = 1.0e-4;

      float qclus = cluster.charge();     
      float locBz = theParam->bz;
      //cout << "PixelCPEGeneric::localPosition(...) : locBz = " << locBz << endl;
      
      pixmx  = -999.9; // max pixel charge for truncation of 2-D cluster
      sigmay = -999.9; // CPE Generic y-error for multi-pixel cluster
      deltay = -999.9; // CPE Generic y-bias for multi-pixel cluster
      sigmax = -999.9; // CPE Generic x-error for multi-pixel cluster
      deltax = -999.9; // CPE Generic x-bias for multi-pixel cluster
      sy1    = -999.9; // CPE Generic y-error for single single-pixel
      dy1    = -999.9; // CPE Generic y-bias for single single-pixel cluster
      sy2    = -999.9; // CPE Generic y-error for single double-pixel cluster
      dy2    = -999.9; // CPE Generic y-bias for single double-pixel cluster
      sx1    = -999.9; // CPE Generic x-error for single single-pixel cluster
      dx1    = -999.9; // CPE Generic x-bias for single single-pixel cluster
      sx2    = -999.9; // CPE Generic x-error for single double-pixel cluster
      dx2    = -999.9; // CPE Generic x-bias for single double-pixel cluster
  
      if(useNewSimplerErrors) { // errors from new light templates 
	gtemplID_ = genErrorDBObject_->getGenErrorID(theParam->theDet->geographicalId().rawId());
	qBin_ = gtempl_.qbin( gtemplID_, cotalpha_, cotbeta_, locBz, qclus,  // inputs
			      pixmx,                                       // returned by reference
			      sigmay, deltay, sigmax, deltax,              // returned by reference
			      sy1, dy1, sy2, dy2, sx1, dx1, sx2, dx2 );    // returned by reference
	

	// OK, now use the charge widths stored in the new generic template headers (change to the
	// incorrect sign convention of the base class)       
	chargeWidthX = -micronsToCm*gtempl_.lorxwidth();
	chargeWidthY = -micronsToCm*gtempl_.lorywidth();

      } else { // errors from full templates 

	templID_ = templateDBobject_->getTemplateID(theParam->theDet->geographicalId().rawId());
	qBin_ = templ_.qbin( templID_, cotalpha_, cotbeta_, locBz, qclus,  // inputs
			     pixmx,                                       // returned by reference
			     sigmay, deltay, sigmax, deltax,              // returned by reference
			     sy1, dy1, sy2, dy2, sx1, dx1, sx2, dx2 );    // returned by reference

      }  // if iseNewSimplerErrors

      // These numbers come in microns from the qbin(...) call. Transform them to cm.
      deltax = deltax * micronsToCm;
      dx1 = dx1 * micronsToCm;
      dx2 = dx2 * micronsToCm;
      
      deltay = deltay * micronsToCm;
      dy1 = dy1 * micronsToCm;
      dy2 = dy2 * micronsToCm;
      
      sigmax = sigmax * micronsToCm;
      sx1 = sx1 * micronsToCm;
      sx2 = sx2 * micronsToCm;
      
      sigmay = sigmay * micronsToCm;
      sy1 = sy1 * micronsToCm;
      sy2 = sy2 * micronsToCm;
      
  } // if ( UseErrorsFromTemplates_ )
  
  float Q_f_X = 0.0;        //!< Q of the first  pixel  in X 
  float Q_l_X = 0.0;        //!< Q of the last   pixel  in X
  float Q_f_Y = 0.0;        //!< Q of the first  pixel  in Y 
  float Q_l_Y = 0.0;        //!< Q of the last   pixel  in Y
  collect_edge_charges( cluster, 
			Q_f_X, Q_l_X, 
			Q_f_Y, Q_l_Y );

  //--- Find the inner widths along X and Y in one shot.  We
  //--- compute the upper right corner of the inner pixels
  //--- (== lower left corner of upper right pixel) and
  //--- the lower left corner of the inner pixels
  //--- (== upper right corner of lower left pixel), and then
  //--- subtract these two points in the formula.

  //--- Upper Right corner of Lower Left pixel -- in measurement frame
  MeasurementPoint meas_URcorn_LLpix( cluster.minPixelRow()+1.0,
				      cluster.minPixelCol()+1.0 );

  //--- Lower Left corner of Upper Right pixel -- in measurement frame
  MeasurementPoint meas_LLcorn_URpix( cluster.maxPixelRow(),
				      cluster.maxPixelCol() );

  //--- These two now converted into the local  
  LocalPoint local_URcorn_LLpix;
  LocalPoint local_LLcorn_URpix;

  // PixelCPEGeneric can be used with or without track angles
  // If PixelCPEGeneric is called with track angles, use them to correct for bows/kinks:
  if ( with_track_angle ) {
    local_URcorn_LLpix = theParam->theTopol->localPosition(meas_URcorn_LLpix, loc_trk_pred_);
    local_LLcorn_URpix = theParam->theTopol->localPosition(meas_LLcorn_URpix, loc_trk_pred_);
  } else {
    local_URcorn_LLpix = theParam->theTopol->localPosition(meas_URcorn_LLpix);
    local_LLcorn_URpix = theParam->theTopol->localPosition(meas_LLcorn_URpix);
  }

 #ifdef EDM_ML_DEBUG
  if (theVerboseLevel > 20) {
    cout  
      << "\n\t >>> cluster.x = " << cluster.x()
      << "\n\t >>> cluster.y = " << cluster.y()
      << "\n\t >>> cluster: minRow = " << cluster.minPixelRow()
      << "  minCol = " << cluster.minPixelCol()
      << "\n\t >>> cluster: maxRow = " << cluster.maxPixelRow()
      << "  maxCol = " << cluster.maxPixelCol()
      << "\n\t >>> meas: inner lower left  = " << meas_URcorn_LLpix.x() 
      << "," << meas_URcorn_LLpix.y()
      << "\n\t >>> meas: inner upper right = " << meas_LLcorn_URpix.x() 
      << "," << meas_LLcorn_URpix.y() 
      << endl;
  }
#endif

  //--- &&& Note that the cuts below should not be hardcoded (like in Orca and
  //--- &&& CPEFromDetPosition/PixelCPEInitial), but rather be
  //--- &&& externally settable (but tracked) parameters.  

  //--- Position, including the half lorentz shift

 #ifdef EDM_ML_DEBUG
  if (theVerboseLevel > 20) 
    cout << "\t >>> Generic:: processing X" << endl;
#endif

  float xPos = 
    generic_position_formula( cluster.sizeX(),
			      Q_f_X, Q_l_X, 
			      local_URcorn_LLpix.x(), local_LLcorn_URpix.x(),
			      chargeWidthX,   // lorentz shift in cm
			      cotalpha_,
			      theParam->thePitchX,
			      theParam->theRecTopol->isItBigPixelInX( cluster.minPixelRow() ),
			      theParam->theRecTopol->isItBigPixelInX( cluster.maxPixelRow() ),
			      the_eff_charge_cut_lowX,
                              the_eff_charge_cut_highX,
                              the_size_cutX);           // cut for eff charge width &&&


  // apply the lorentz offset correction 			     
  xPos = xPos + shiftX;

#ifdef EDM_ML_DEBUG
  if (theVerboseLevel > 20) 
    cout << "\t >>> Generic:: processing Y" << endl;
#endif

  float yPos = 
    generic_position_formula( cluster.sizeY(),
			      Q_f_Y, Q_l_Y, 
			      local_URcorn_LLpix.y(), local_LLcorn_URpix.y(),
			      chargeWidthY,   // lorentz shift in cm
			      cotbeta_,
			      theParam->thePitchY,  
			      theParam->theRecTopol->isItBigPixelInY( cluster.minPixelCol() ),
			      theParam->theRecTopol->isItBigPixelInY( cluster.maxPixelCol() ),
			      the_eff_charge_cut_lowY,
                              the_eff_charge_cut_highY,
                              the_size_cutY);           // cut for eff charge width &&&

  // apply the lorentz offset correction 			     
  yPos = yPos + shiftY;

  // Apply irradiation corrections. NOT USED FOR NOW
  if ( IrradiationBiasCorrection_ ) {
    if ( cluster.sizeX() == 1 ) {  // size=1	  
      // ggiurgiu@jhu.edu, 02/03/09 : for size = 1, the Lorentz shift is already accounted by the irradiation correction
      xPos = xPos - (0.5 * lorentzShiftInCmX_);
      // Find if pixel is double (big). 
      bool bigInX = theParam->theRecTopol->isItBigPixelInX( cluster.maxPixelRow() );	  
      if ( !bigInX ) xPos -= dx1;	   
      else           xPos -= dx2;
	  
    } else { // size>1
      //cout << "Apply correction correction_deltax = " << deltax << " to xPos = " << xPos << endl;
      xPos -= deltax;
    }
    
    if ( cluster.sizeY() == 1 ) {
      // ggiurgiu@jhu.edu, 02/03/09 : for size = 1, the Lorentz shift is already accounted by the irradiation correction
      yPos = yPos - (0.5 * lorentzShiftInCmY_);
      
      // Find if pixel is double (big). 
      bool bigInY = theParam->theRecTopol->isItBigPixelInY( cluster.maxPixelCol() );      
      if ( !bigInY ) yPos -= dy1;
      else           yPos -= dy2;
      
    } else { 
      yPos -= deltay; 
    }
 
  } // if ( IrradiationBiasCorrection_ )
	
  //cout<<" in PixelCPEGeneric:localPosition - pos = "<<xPos<<" "<<yPos<<endl; //dk

  //--- Now put the two together
  LocalPoint pos_in_local( xPos, yPos );
  return pos_in_local;
}



//-----------------------------------------------------------------------------
//!  A generic version of the position formula.  Since it works for both
//!  X and Y, in the interest of the simplicity of the code, all parameters
//!  are passed by the caller.  The only class variable used by this method
//!  is the theThickness, since that's common for both X and Y.
//-----------------------------------------------------------------------------
float
PixelCPEGeneric::    
generic_position_formula( int size,                //!< Size of this projection.
			  float Q_f,              //!< Charge in the first pixel.
			  float Q_l,              //!< Charge in the last pixel.
			  float upper_edge_first_pix, //!< As the name says.
			  float lower_edge_last_pix,  //!< As the name says.
			  float lorentz_shift,   //!< L-shift at half thickness
			  float cot_angle,        //!< cot of alpha_ or beta_
			  float pitch,            //!< thePitchX or thePitchY
			  bool first_is_big,       //!< true if the first is big
			  bool last_is_big,        //!< true if the last is big
			  float eff_charge_cut_low, //!< Use edge if > W_eff  &&&
			  float eff_charge_cut_high,//!< Use edge if < W_eff  &&&
			  float size_cut         //!< Use edge when size == cuts
			 ) const
{

  //cout<<" in PixelCPEGeneric:generic_position_formula - "<<endl; //dk

  float geom_center = 0.5f * ( upper_edge_first_pix + lower_edge_last_pix );

  //--- The case of only one pixel in this projection is separate.  Note that
  //--- here first_pix == last_pix, so the average of the two is still the
  //--- center of the pixel.
  if ( size == 1 ) {return geom_center;}

  //--- Width of the clusters minus the edge (first and last) pixels.
  //--- In the note, they are denoted x_F and x_L (and y_F and y_L)
  float W_inner      = lower_edge_last_pix - upper_edge_first_pix;  // in cm

  //--- Predicted charge width from geometry
  float W_pred = 
    theParam->theThickness * cot_angle                     // geometric correction (in cm)
    - lorentz_shift;                    // (in cm) &&& check fpix!  

  //cout<<" in PixelCPEGeneric:generic_position_formula - "<<W_inner<<" "<<W_pred<<endl; //dk

  //--- Total length of the two edge pixels (first+last)
  float sum_of_edge = 2.0f;
  if (first_is_big) sum_of_edge += 1.0f;
  if (last_is_big)  sum_of_edge += 1.0f;
  

  //--- The `effective' charge width -- particle's path in first and last pixels only
  float W_eff = std::abs( W_pred ) - W_inner;


  //--- If the observed charge width is inconsistent with the expectations
  //--- based on the track, do *not* use W_pred-W_innner.  Instead, replace
  //--- it with an *average* effective charge width, which is the average
  //--- length of the edge pixels.
  //
  //  bool usedEdgeAlgo = false;
  if ( (size >= size_cut) || (
       ( W_eff/pitch < eff_charge_cut_low ) |
       ( W_eff/pitch > eff_charge_cut_high ) ) ) 
    {
      W_eff = pitch * 0.5f * sum_of_edge;  // ave. length of edge pixels (first+last) (cm)
      //  usedEdgeAlgo = true;
      nRecHitsUsedEdge_++;
    }

  
  //--- Finally, compute the position in this projection
  float Qdiff = Q_l - Q_f;
  float Qsum  = Q_l + Q_f;

  //--- Temporary fix for clusters with both first and last pixel with charge = 0
  if(Qsum==0) Qsum=1.0f;
  //float hit_pos = geom_center + 0.5f*(Qdiff/Qsum) * W_eff + half_lorentz_shift;
  float hit_pos = geom_center + 0.5f*(Qdiff/Qsum) * W_eff;

  //cout<<" in PixelCPEGeneric:generic_position_formula - "<<hit_pos<<" "<<lorentz_shift*0.5<<endl; //dk

 #ifdef EDM_ML_DEBUG
  //--- Debugging output
  if (theVerboseLevel > 20) {
    if ( theParam->thePart == GeomDetEnumerators::PixelBarrel ) {
      cout << "\t >>> We are in the Barrel." ;
    } else {
      cout << "\t >>> We are in the Forward." ;
    }
    cout 
      << "\n\t >>> cot(angle) = " << cot_angle << "  pitch = " << pitch << "  size = " << size
      << "\n\t >>> upper_edge_first_pix = " << upper_edge_first_pix
      << "\n\t >>> lower_edge_last_pix  = " << lower_edge_last_pix
      << "\n\t >>> geom_center          = " << geom_center
      << "\n\t >>> half_lorentz_shift   = " << half_lorentz_shift
      << "\n\t >>> W_inner              = " << W_inner
      << "\n\t >>> W_pred               = " << W_pred
      << "\n\t >>> W_eff(orig)          = " << fabs( W_pred ) - W_inner
      << "\n\t >>> W_eff(used)          = " << W_eff
      << "\n\t >>> sum_of_edge          = " << sum_of_edge
      << "\n\t >>> Qdiff = " << Qdiff << "  Qsum = " << Qsum 
      << "\n\t >>> hit_pos              = " << hit_pos 
      << "\n\t >>> RecHits: total = " << nRecHitsTotal_ 
      << "  used edge = " << nRecHitsUsedEdge_
      << endl;
    if (usedEdgeAlgo) 
      cout << "\n\t >>> Used Edge algorithm." ;
    else
      cout << "\n\t >>> Used angle information." ;
    cout << endl;
  }
#endif

  return hit_pos;
}


//-----------------------------------------------------------------------------
//!  Collect the edge charges in x and y, in a single pass over the pixel vector.
//!  Calculate charge in the first and last pixel projected in x and y
//!  and the inner cluster charge, projected in x and y.
//-----------------------------------------------------------------------------
void
PixelCPEGeneric::
collect_edge_charges(const SiPixelCluster& cluster,  //!< input, the cluster
		     float & Q_f_X,              //!< output, Q first  in X 
		     float & Q_l_X,              //!< output, Q last   in X
		     float & Q_f_Y,              //!< output, Q first  in Y 
		     float & Q_l_Y               //!< output, Q last   in Y
		     ) const
{
  // Initialize return variables.
  Q_f_X = Q_l_X = 0.0;
  Q_f_Y = Q_l_Y = 0.0;


  // Obtain boundaries in index units
  int xmin = cluster.minPixelRow();
  int xmax = cluster.maxPixelRow();
  int ymin = cluster.minPixelCol();
  int ymax = cluster.maxPixelCol();


  // Iterate over the pixels.
  int isize = cluster.size();
  for (int i = 0;  i != isize; ++i) 
    {
      auto const & pixel = cluster.pixel(i); 
      // ggiurgiu@fnal.gov: add pixel charge truncation
      float pix_adc = pixel.adc;
      if ( UseErrorsFromTemplates_ && TruncatePixelCharge_ ) 
	pix_adc = std::min(pix_adc, pixmx );

      //
      // X projection
      if ( pixel.x == xmin ) Q_f_X += pix_adc;
      if ( pixel.x == xmax ) Q_l_X += pix_adc;
      //
      // Y projection
      if ( pixel.y == ymin ) Q_f_Y += pix_adc;
      if ( pixel.y == ymax ) Q_l_Y += pix_adc;
    }
  
  return;
} 


//==============  INFLATED ERROR AND ERRORS FROM DB BELOW  ================

//-------------------------------------------------------------------------
//  Hit error in the local frame
//-------------------------------------------------------------------------
LocalError 
PixelCPEGeneric::localError( const SiPixelCluster& cluster) const 
{
  const bool localPrint = false;
  // Default errors are the maximum error used for edge clusters.
  // These are determined by looking at residuals for edge clusters
  const float micronsToCm = 1.0e-4f;
  float xerr = EdgeClusterErrorX_ * micronsToCm;
  float yerr = EdgeClusterErrorY_ * micronsToCm;
  

  // Find if cluster is at the module edge. 
  int maxPixelCol = cluster.maxPixelCol();
  int maxPixelRow = cluster.maxPixelRow();
  int minPixelCol = cluster.minPixelCol();
  int minPixelRow = cluster.minPixelRow();       

  bool edgex = ( theParam->theRecTopol->isItEdgePixelInX( minPixelRow ) ) || ( theParam->theRecTopol->isItEdgePixelInX( maxPixelRow ) );
  bool edgey = ( theParam->theRecTopol->isItEdgePixelInY( minPixelCol ) ) || ( theParam->theRecTopol->isItEdgePixelInY( maxPixelCol ) );

  unsigned int sizex = cluster.sizeX();
  unsigned int sizey = cluster.sizeY();
  if( int(sizex) != (maxPixelRow - minPixelRow+1) ) cout<<" wrong x"<<endl;
  if( int(sizey) != (maxPixelCol - minPixelCol+1) ) cout<<" wrong y"<<endl;

  // Find if cluster contains double (big) pixels. 
  bool bigInX = theParam->theRecTopol->containsBigPixelInX( minPixelRow, maxPixelRow ); 	 
  bool bigInY = theParam->theRecTopol->containsBigPixelInY( minPixelCol, maxPixelCol );

  if(localPrint) {
  cout<<" endge clus "<<xerr<<" "<<yerr<<endl;  //dk
  if(bigInX || bigInY) cout<<" big "<<bigInX<<" "<<bigInY<<endl;
  if(edgex || edgey) cout<<" edge "<<edgex<<" "<<edgey<<endl;
  cout<<" before if "<<UseErrorsFromTemplates_<<" "<<qBin_<<endl;
  if(qBin_ == 0) 
    cout<<" qbin 0! "<<edgex<<" "<<edgey<<" "<<bigInX<<" "<<bigInY<<" "
	<<sizex<<" "<<sizey<<endl;
  if(sizex==2 && sizey==2) cout<<" size 2*2 "<<qBin_<<endl;
  }

  //if likely(UseErrorsFromTemplates_ && (qBin_!= 0) ) {
  if likely(UseErrorsFromTemplates_ ) {
      //
      // Use template errors 
      //cout << "Track angles are known. We can use either errors from templates or the error parameterization from DB." << endl;
      

      if ( !edgex ) { // Only use this for non-edge clusters
	if ( sizex == 1 ) {
	  if ( !bigInX ) {xerr = sx1;} 
	  else           {xerr = sx2;}
	} else {xerr = sigmax;}		  
      }
	      
      if ( !edgey ) { // Only use for non-edge clusters
	if ( sizey == 1 ) {
	  if ( !bigInY ) {yerr = sy1;}
	  else           {yerr = sy2;}
	} else {yerr = sigmay;}		  
      }

      if(localPrint) {
      cout<<" in if "<<edgex<<" "<<edgey<<" "<<sizex<<" "<<sizey<<endl;
      cout<<" errors  "<<xerr<<" "<<yerr<<" "<<sx1<<" "<<sx2<<" "<<sigmax<<endl;  //dk
      }

  } else  { // simple errors

    // This are the simple errors, hardcoded in the code 
    cout << "Track angles are not known and we are processing cosmics." << endl; 
    //cout << "Default angle estimation which assumes track from PV (0,0,0) does not work." << endl;
      
    if ( theParam->thePart == GeomDetEnumerators::PixelBarrel )  {

      DetId id = (theParam->theDet->geographicalId());
      int layer=PXBDetId(id).layer();
      if ( layer==1 ) {
	if ( !edgex ) {
	  if ( sizex<=xerr_barrel_l1_.size() ) xerr=xerr_barrel_l1_[sizex-1];
	  else xerr=xerr_barrel_l1_def_;
	}
	
	if ( !edgey ) {
	  if ( sizey<=yerr_barrel_l1_.size() ) yerr=yerr_barrel_l1_[sizey-1];
	  else yerr=yerr_barrel_l1_def_;
	}
      } else{  // layer 2,3
	if ( !edgex ) {
	  if ( sizex<=xerr_barrel_ln_.size() ) xerr=xerr_barrel_ln_[sizex-1];
	  else xerr=xerr_barrel_ln_def_;
	}
	    
	if ( !edgey ) {
	  if ( sizey<=yerr_barrel_ln_.size() ) yerr=yerr_barrel_ln_[sizey-1];
	  else yerr=yerr_barrel_ln_def_;
	}
      }

    } else { // EndCap

      if ( !edgex ) {
	if ( sizex<=xerr_endcap_.size() ) xerr=xerr_endcap_[sizex-1];
	else xerr=xerr_endcap_def_;
      }
      
      if ( !edgey ) {
	if ( sizey<=yerr_endcap_.size() ) yerr=yerr_endcap_[sizey-1];
	else yerr=yerr_endcap_def_;
      }
    } // end endcap

    if(inflate_errors) {
      int n_bigx = 0;
      int n_bigy = 0;
      
      for (int irow = 0; irow < 7; ++irow) {
	if ( theParam->theRecTopol->isItBigPixelInX( irow+minPixelRow ) ) ++n_bigx;
      }
      
      for (int icol = 0; icol < 21; ++icol) {
	  if ( theParam->theRecTopol->isItBigPixelInY( icol+minPixelCol ) ) ++n_bigy;
      }
      
      xerr = (float)(sizex + n_bigx) * theParam->thePitchX / std::sqrt( 12.0f );
      yerr = (float)(sizey + n_bigy) * theParam->thePitchY / std::sqrt( 12.0f );
      
    } // if(inflate_errors)

  } // end 

#ifdef EDM_ML_DEBUG
  if ( !(xerr > 0.0) )
    throw cms::Exception("PixelCPEGeneric::localError") 
      << "\nERROR: Negative pixel error xerr = " << xerr << "\n\n";
  
  if ( !(yerr > 0.0) )
    throw cms::Exception("PixelCPEGeneric::localError") 
      << "\nERROR: Negative pixel error yerr = " << yerr << "\n\n";
#endif
 
  if(localPrint) {
    cout<<" errors  "<<xerr<<" "<<yerr<<endl;  //dk
    if(qBin_ == 0) cout<<" qbin 0 "<<xerr<<" "<<yerr<<endl;
  }

  auto xerr_sq = xerr*xerr; 
  auto yerr_sq = yerr*yerr;
  
  return LocalError( xerr_sq, 0, yerr_sq );

}






