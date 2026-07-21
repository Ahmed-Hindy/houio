#pragma once
#include <houio/Diagnostic.h>
#include <houio/HouGeo.h>
#include <houio/Geometry.h>


namespace houio
{
	class GeometryIO;

	struct HouGeoIO
	{
		static HouGeo::Ptr                      import( std::istream *in );
		static HouGeo::Ptr                      import( std::istream *in, DiagnosticList *diagnostics );
		static HouGeo::Ptr                      import( std::istream *in, const json::ParserLimits &limits );
		static HouGeo::Ptr                      import( std::istream *in, const json::ParserLimits &limits, DiagnosticList *diagnostics );
		static Geometry::Ptr                    importGeometry( const std::string &path );
		static Geometry::Ptr                    importGeometry( const std::string &path, DiagnosticList *diagnostics );
		static ScalarField::Ptr                 importVolume(const std::string &path);
		static ScalarField::Ptr                 importVolume(const std::string &path, DiagnosticList *diagnostics);
		static void                             makeLog( const std::string &path, std::ostream *output );

		// Lossy convenience conversion. Requires P, one fixed polygon size, and domain-consistent attributes.
		// Vertex attributes are flattened to points by duplicating points where values differ.
		static Geometry::Ptr                    convertToGeometry(HouGeo::Ptr houdiniGeometry, HouGeoAdapter::Primitive::Ptr primitive );
		static Geometry::Ptr                    convertToGeometry(HouGeo::Ptr houdiniGeometry, HouGeoAdapter::Primitive::Ptr primitive, DiagnosticList *diagnostics );

		static bool                             exportVolume( const std::string &filename, ScalarField::Ptr volume );
		static bool                             exportGeometry( const std::string &filename, Geometry::Ptr geometry );
		static bool                             exportGeometry( std::ostream *output, HouGeoAdapter::Ptr geometry, bool binary = true );

		// Historical compatibility wrappers. New code should use exportVolume() or exportGeometry().
		static bool                             xport( const std::string& filename, ScalarField::Ptr volume );
		static bool                             xport( const std::string& filename, Geometry::Ptr geometry );
		static bool                             xport( const std::string& filename, const std::vector<math::V3f>& points );
		static bool                             xport( const std::string& filename, const std::map<std::string, std::vector<math::V3f>>& pointAttributes );
		static bool                             xport( std::ostream *output, HouGeoAdapter::Ptr geometry, bool binary = true );

	private:
		friend class GeometryIO;

		static HouGeo::Ptr                      adaptVolume( ScalarField::Ptr volume );
		static HouGeo::Ptr                      adaptGeometry( Geometry::Ptr geometry );

		struct ExportContext
		{
			explicit ExportContext( json::BinaryWriter &activeWriter ) : writer(activeWriter) {}
			json::BinaryWriter &writer;
		};

		static bool                             exportAttribute( ExportContext &context, HouGeoAdapter::AttributeAdapter::Ptr attribute );
		static bool                             exportTopology( ExportContext &context, HouGeoAdapter::Topology::Ptr topology );
		static bool                             exportPrimitive( ExportContext &context, HouGeoAdapter::VolumePrimitive::Ptr volume );
		static bool                             exportPrimitive( ExportContext &context, HouGeoAdapter::PolyPrimitive::Ptr polygonRun, int startVertex );
		static bool                             exportGroup( ExportContext &context, const std::string &name, const std::vector<bool> &membership );
	};
}
