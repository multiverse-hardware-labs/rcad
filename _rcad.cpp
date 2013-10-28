#include <gp_Pnt2d.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepBuilderAPI_MakeEdge2d.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <StlAPI_Writer.hxx>
#include <Standard_Failure.hxx>
#include <rice/Class.hpp>
#include <rice/Exception.hpp>
#include <rice/Array.hpp>

using namespace Rice;


Data_Type<Standard_Failure> rb_cOCEError;
Class rb_cShape;


void translate_oce_exception(const Standard_Failure &e)
{
    //Data_Object<Standard_Failure> e_obj(
    //    new Standard_Failure(e), rb_cOCEError);
    throw Exception(rb_cOCEError, "%s", e.GetMessageString());
}


static Data_Object<TopoDS_Shape> render_shape(Object shape)
{
    do {
        shape = shape.call("render");
    } while (shape.is_a(rb_cShape));
    
    if (shape.is_nil()) {
        throw Exception(rb_eArgError, "render returned nil");
    }

    return shape;
}

Object shape_move(Object self, Standard_Real x, Standard_Real y,
    Standard_Real z)
{
    gp_Trsf transform;
    transform.SetTranslation(gp_Vec(x, y, z));

    Data_Object<TopoDS_Shape> rendered = render_shape(self);
    TopoDS_Shape new_shape(
        BRepBuilderAPI_Transform(*rendered, transform, Standard_True).Shape());

    Object shape_obj = rb_cShape.call("new");
    shape_obj.iv_set("@shape", to_ruby(new_shape));
    return shape_obj;
}

Object shape_rotate(Object self, Standard_Real angle, Array axis)
{
    return self;
}

Object shape_scale(Object self, Standard_Real x, Standard_Real y,
    Standard_Real z)
{
    return self;
}

void shape_write_stl(Object self, String path)
{
    Data_Object<TopoDS_Shape> shape = render_shape(self);

    StlAPI_Writer writer;
    writer.ASCIIMode() = false;
    writer.RelativeMode() = false;
    writer.SetDeflection(0.05);     // TODO: deflection param
    writer.Write(*shape, path.c_str());
}


static gp_Pnt2d from_ruby_pnt2d(Object obj)
{
    Array ary(obj);

    if (ary.size() != 2) {
        throw Exception(rb_eArgError,
            "2D points must be arrays with 2 numbers each");
    }

    return gp_Pnt2d(
        from_ruby<Standard_Real>(ary[0]),
        from_ruby<Standard_Real>(ary[1]));
}


void polygon_initialize(Object self, Array points, Object paths)
{
    self.iv_set("@points", points);
    self.iv_set("@paths", paths);

    if (paths.is_nil()) {
        BRepBuilderAPI_MakeWire wire_maker;

        for (size_t i = 0; i < points.size(); ++i) {
            const size_t j = (i + 1) % points.size();

            gp_Pnt2d gp_p1(from_ruby_pnt2d(points[i]));
            gp_Pnt2d gp_p2(from_ruby_pnt2d(points[j]));

            wire_maker.Add(
                BRepBuilderAPI_MakeEdge2d(gp_p1, gp_p2).Edge());
        }

        self.iv_set("@shape",
            BRepBuilderAPI_MakeFace(wire_maker.Wire()).Shape());
    } else {
        // TODO
    }
}


void box_initialize(Object self, double xsize, double ysize, double zsize)
{
    self.iv_set("@xsize", xsize);
    self.iv_set("@ysize", ysize);
    self.iv_set("@zsize", zsize);

    self.iv_set("@shape",
        BRepPrimAPI_MakeBox(xsize, ysize, zsize).Shape());
}


void combination_initialize(Object self, Object a, Object b)
{
    self.iv_set("@a", a);
    self.iv_set("@b", b);
}


Object union_render(Object self)
{
    Data_Object<TopoDS_Shape> shape_a = render_shape(self.iv_get("@a"));
    Data_Object<TopoDS_Shape> shape_b = render_shape(self.iv_get("@b"));
    return to_ruby(
        BRepAlgoAPI_Fuse(*shape_a, *shape_b).Shape());
}


Object difference_render(Object self)
{
    Data_Object<TopoDS_Shape> shape_a = render_shape(self.iv_get("@a"));
    Data_Object<TopoDS_Shape> shape_b = render_shape(self.iv_get("@b"));
    return to_ruby(
        BRepAlgoAPI_Cut(*shape_a, *shape_b).Shape());
}


Object intersection_render(Object self)
{
    Data_Object<TopoDS_Shape> shape_a = render_shape(self.iv_get("@a"));
    Data_Object<TopoDS_Shape> shape_b = render_shape(self.iv_get("@b"));
    return to_ruby(
        BRepAlgoAPI_Common(*shape_a, *shape_b).Shape());
}


// initialize is defined in Ruby code
Object linear_extrusion_render(Object self)
{
    Object profile = self.iv_get("@profile");
    Standard_Real height = from_ruby<Standard_Real>(self.iv_get("@height"));
    Standard_Real twist = from_ruby<Standard_Real>(self.iv_get("@twist"));

    Data_Object<TopoDS_Shape> shape = render_shape(profile);

    if (0 == twist) {
        return to_ruby(
            BRepPrimAPI_MakePrism(*shape, gp_Vec(0, 0, height),
                Standard_True).Shape());
    } else {
        // TODO
    }

    return Object();
}


// initialize is defined in Ruby code
Object revolution_render(Object self)
{
    Object profile = self.iv_get("@profile");
    Data_Object<TopoDS_Shape> shape = render_shape(profile);
    
    Object angle = self.iv_get("@angle");
    if (angle.is_nil()) {
        return to_ruby(
            BRepPrimAPI_MakeRevol(*shape, gp::OY(), Standard_True).Shape());
    } else {
        Standard_Real angle_num = from_ruby<Standard_Real>(angle);
        return to_ruby(
            BRepPrimAPI_MakeRevol(*shape, gp::OY(), angle_num,
                Standard_True).Shape());
    }
}


extern "C"
void Init__rcad()
{
    Data_Type<TopoDS_Shape> rb_cRenderedShape =
        define_class<TopoDS_Shape>("RenderedShape");

    Data_Type<Standard_Failure> rb_cOCEError =
        define_class("rb_cOCEError", rb_eRuntimeError);

    rb_cShape = define_class("Shape")
        .add_handler<Standard_Failure>(translate_oce_exception)
        .define_method("move", &shape_move)
        .define_method("rotate", &shape_rotate)
        .define_method("scale", &shape_scale)
        .define_method("write_stl", &shape_write_stl);

    Class rb_cPolygon = define_class("Polygon", rb_cShape)
        .add_handler<Standard_Failure>(translate_oce_exception)
        .define_method("initialize", &polygon_initialize,
            (Arg("points"), Arg("paths") = Object(Qnil)));

    Class rb_cBox = define_class("Box", rb_cShape)
        .add_handler<Standard_Failure>(translate_oce_exception)
        .define_method("initialize", &box_initialize);

    Class rb_cCombination = define_class("Combination", rb_cShape)
        .add_handler<Standard_Failure>(translate_oce_exception)
        .define_method("initialize", &combination_initialize);

    Class rb_cUnion = define_class("Union", rb_cCombination)
        .add_handler<Standard_Failure>(translate_oce_exception)
        .define_method("render", &union_render);

    Class rb_cDifference = define_class("Difference", rb_cCombination)
        .add_handler<Standard_Failure>(translate_oce_exception)
        .define_method("render", &difference_render);

    Class rb_cIntersection = define_class("Intersection", rb_cCombination)
        .add_handler<Standard_Failure>(translate_oce_exception)
        .define_method("render", &intersection_render);

    Class rb_cLinearExtrusion = define_class("LinearExtrusion", rb_cShape)
        .add_handler<Standard_Failure>(translate_oce_exception)
        .define_method("render", &linear_extrusion_render);

    Class rb_cRevolution = define_class("Revolution", rb_cShape)
        .add_handler<Standard_Failure>(translate_oce_exception)
        .define_method("render", &revolution_render);
}
