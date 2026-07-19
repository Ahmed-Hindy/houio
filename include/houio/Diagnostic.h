#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <houio/types.h>

namespace houio
{
	enum class DiagnosticSeverity
	{
		warning,
		error
	};

	enum class DiagnosticCategory
	{
		io,
		malformed_input,
		unsupported_input,
		schema,
		conversion
	};

	struct Diagnostic
	{
		DiagnosticSeverity severity = DiagnosticSeverity::error;
		DiagnosticCategory category = DiagnosticCategory::malformed_input;
		std::string message;
		sint64 byteOffset = -1;
		std::string path;
	};

	using DiagnosticList = std::vector<Diagnostic>;

	class DiagnosticException : public std::runtime_error
	{
	public:
		explicit DiagnosticException( Diagnostic diagnostic ) :
			std::runtime_error(diagnostic.message),
			m_diagnostic(std::move(diagnostic))
		{
		}

		const Diagnostic &diagnostic()const noexcept
		{
			return m_diagnostic;
		}

	private:
		Diagnostic m_diagnostic;
	};

	inline void appendDiagnostic( DiagnosticList *diagnostics, Diagnostic diagnostic )
	{
		if( diagnostics )
			diagnostics->push_back(std::move(diagnostic));
	}

	inline Diagnostic withDiagnosticPath( Diagnostic diagnostic, const std::string &path )
	{
		if( diagnostic.path.empty() )
			diagnostic.path = path;
		else if( !path.empty() )
			diagnostic.path = path + "." + diagnostic.path;
		return diagnostic;
	}
}
