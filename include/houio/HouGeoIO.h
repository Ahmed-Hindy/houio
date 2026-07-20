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
		static void                             makeLog( const std::string &path, std::ostream *out );

		// Lossy convenience conversion. Requires P, one fixed polygon size, and domain-consistent attributes.
		// Vertex attributes are flattened to points by duplicating points where values differ.
		static Geometry::Ptr                    convertToGeometry(HouGeo::Ptr houGeo, HouGeoAdapter::Primitive::Ptr houPrim );
		static Geometry::Ptr                    convertToGeometry(HouGeo::Ptr houGeo, HouGeoAdapter::Primitive::Ptr houPrim, DiagnosticList *diagnostics );

		static bool                             exportVolume( const std::string &filename, ScalarField::Ptr volume );
		static bool                             exportGeometry( const std::string &filename, Geometry::Ptr geo );
		static bool                             exportGeometry( std::ostream *out, HouGeoAdapter::Ptr geo, bool binary = true );

		// Historical compatibility wrappers. New code should use exportVolume() or exportGeometry().
		static bool                             xport( const std::string& filename, ScalarField::Ptr volume );
		static bool                             xport( const std::string& filename, Geometry::Ptr geo );
		static bool                             xport( const std::string& filename, const std::vector<math::V3f>& points );
		static bool                             xport( const std::string& filename, const std::map<std::string, std::vector<math::V3f>>& pattr_v3f );
		static bool                             xport( std::ostream *out, HouGeoAdapter::Ptr geo, bool binary = true );

	private:
		friend class GeometryIO;

		static HouGeo::Ptr                      adaptVolume( ScalarField::Ptr volume );
		static HouGeo::Ptr                      adaptGeometry( Geometry::Ptr geo );

		struct ExportContext
		{
			explicit ExportContext( json::BinaryWriter &activeWriter ) : writer(activeWriter) {}
			json::BinaryWriter &writer;
		};

		static bool                             exportAttribute( ExportContext &context, HouGeoAdapter::AttributeAdapter::Ptr attr );
		static bool                             exportTopology( ExportContext &context, HouGeoAdapter::Topology::Ptr topo );
		static bool                             exportPrimitive( ExportContext &context, HouGeoAdapter::VolumePrimitive::Ptr volume );
		static bool                             exportPrimitive( ExportContext &context, HouGeoAdapter::PolyPrimitive::Ptr poly, int startVertex );
		static bool                             exportGroup( ExportContext &context, const std::string &name, const std::vector<bool> &membership );
	};
}
