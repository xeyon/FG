#pragma once

/* ------------- A NAV/COMM Frequency formatter ---------------------- */
class FrequencyFormatterBase : public SGReferenced {
public:
	  virtual ~FrequencyFormatterBase()
	  {
	  }

	  virtual double getFrequency() const = 0;

};

class FrequencyFormatter : public FrequencyFormatterBase, public SGPropertyChangeListener {
public:
  FrequencyFormatter( SGPropertyNode_ptr freqNode, SGPropertyNode_ptr fmtFreqNode, double channelSpacing, double min, double max ) :
    _freqNode( freqNode ),
    _fmtFreqNode( fmtFreqNode ),
    _channelSpacing(channelSpacing),
    _min(min),
    _max(max)
  {
    _freqNode->addChangeListener( this, true );
  }
  virtual ~FrequencyFormatter()
  {
    _freqNode->removeChangeListener( this );
  }

  void valueChanged (SGPropertyNode * prop)
  {
    // format as fixed decimal "nnn.nn"
    std::ostringstream buf;
    buf << std::fixed 
        << std::setw(5) 
        << std::setfill('0') 
        << std::setprecision(2)
        << getFrequency();
    _fmtFreqNode->setStringValue( buf.str() );
  }

  virtual double getFrequency() const
  {
    double d = SGMiscd::roundToInt(_freqNode->getDoubleValue() / _channelSpacing) * _channelSpacing;
    // strip last digit, do not round
    double f = ((int)(d*100))/100.0;
    if( f < _min ) return _min;
    if( f >= _max ) return _max;
    return f;
  }

private:
  SGPropertyNode_ptr _freqNode;
  SGPropertyNode_ptr _fmtFreqNode;
  double _channelSpacing;
  double _min;
  double _max;
};
